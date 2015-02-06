#include <algorithm>
#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>
#include <boost/make_shared.hpp>
#include <pjlib-util.h>

#include <tango/contacts/ContactManager.h>
#include <tango/driver/Registry.h>
#include <tango/media_engine/MediaEngineMessage.h>
#include <tango/xmpp/XmppSessionMessage.h>
#include <tango/xmpp/MediaEngineManager.h>
#include <tango/xmpp/Constants.h>
#include <tango/xmpp/AddressBookSyncTask.h>
#include <tango/account/UserSettings.h>
#include <tango/messaging/MessageRouter.h>
#include <tango/util/MessageJingleThread.h>
#include <tango/pr/monotonic_time.h>
#include <tango/account/UserInfo.h>
#include <tango/session/Utils.h>
#include <tango/util/string_util.h>
#include <tango/pr/Waiter.h>
#include <tango/phone_formatter/PhoneFormatter.h>
#include <tango/util/tune_util.h>
#include <tango/config/GlobalConfig.h>
#include <tango/network/network_service_manager.h>
#include <tango/server_owned_config/ServerOwnedConfigManager.h>
#include <tango/facilitator_request/contact_filter_request.h>
#include <tango/facilitator_request/upload_contacts_request.h>
#include <tango/facilitator_request/new_contact_request.h>
#include <tango/facilitator_request/refresh_tango_contacts_request.h>
#include <tango/threaded_conversation/tc_util.h>
#include <tango/threaded_conversation/TCGeneralManager.h>
#include <tango/xmpp/Constants.h>
#include <tango/corefacade/coremanagement/CoreManager.h>
#include <tango/corefacade/contacts/ContactService.h>
#include <tango/util/driver_util.h>
#include "../xmpp/XmppSessionImpl.h"
#include "../xmpp/invite_request.h"
#include <tango/interface/localization_utility/LocalizationUtility.h>
#include <tango/messages/Invite.h>
#include <tango/interface/telephony/Telephony.h>
#include <tango/stats_collector/stats_collector.h>
#include <tango/social/ProfileSimpleFetcher.h>
#include <tango/interface/devinfo/DevInfo.h>
#include <tango/acme/RefreshMessage.pb.h>
#include <tango/acme/Acme.h>
#include <tango/test/feature_test_event.h> 
#include <tango/mail_validator/MailValidator.h>
#include <tango/dispatcher_thread/DispatcherThread.h>
#include <tango/localized_string/LocalizedStringContainer.h>
#include <tango/countrycodes/CountryCodes.h>
#include <tango/util/container_util.h>                            // SEQ_ERASE

#define SGMODULE sgiggle::module::contacts

using namespace std;
using namespace sgiggle;
using namespace sgiggle::contacts;
using namespace sgiggle::messaging;
using namespace tango::httpme;
using namespace com::tango::facilitator::client::proto ;

#define CONTACT_RESOLVE_TIMEOUT 2 * 60 * 1000 // 2 mins
#define CONTACT_UPDATE_TIMEOUT 8 * 1000 // 8 seconds
#define NUMBER_OF_DIGITS_AS_KEY_FOR_PHONE_NUMBER    5

static const uint64_t WAIT_BLOCKED_LIST_TIMEOUT_IN_MSEC = 15 * 1000; // msec
static const uint64_t DEFAULT_CONTACT_FILTER_REFRESH_MIN_PERIOD = 3 * 24 * 3600; // 3 days in sec
static const uint64_t INVALID_LATEST_CREATED_TIME = -1;

static const char* const CONTACT_FILTER_REFRESH_INTERVAL_KEY = "contactfiltering.over.acme.v2.refresh.interval";
static const char* const ACME_TRIGGERED_CONTACT_FILTER_V2_ENABLED = "contactfiltering.over.acme.v2.enabled";
static const char* const ACME_CONTACT_FILTER_V2_FALLBACK = "contactfiltering.over.acme.v2.fallback";

static const char* const CONTACT_HIGHLIGHT_MAX = "contact.highlight.max";
static const int CONTACT_HIGHLIGHT_MAX_DEFAULT = 10;
static const char* const CONTACT_HIGHLIGHT_PERIOD = "contact.highlight.period";
static const uint64_t CONTACT_HIGHLIGHT_PERIOD_DEFAULT = 48 * 60 * 60;

static const char* const CONTACT_RECOMMENDATION_MAX = "contact.recommendation.max";
static const int CONTACT_RECOMMENDATION_MAX_DEFAULT = 10;
static const char* const CONTACT_RECOMMENDATION_PERIOD = "contact.recommendation.period";
static const uint64_t CONTACT_RECOMMENDATION_PERIOD_DEFAULT = 24 * 60 * 60;

static const char* const CONTACT_HASH_PREFIX_FOR_TANGO_FRIEND = "acct:";
static const char* const CONTACT_LAST_VIEWED_RECOMMENDATION_TIMESTAMP = "contactmanager.last.viewed.recommendation.timestamp";

static const char* const CONTACTFILTERING_ACME_SERVICE_ID = "contactfiltering.v3";
static const char* const GETTANGOCONTACTS_ACME_SERVICE_ID = "gettangocontacts";

static const char* const LAST_RUN_LATEST_CREATED_TIME = "contact.last_run_latest_created_time";

// a macro to help batch the request, so that at most 1 is executed in 5s.
#define batchedRequest(timerId, func, bImmediately)                                     \
  LET(processor, sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessorImpl()); \
  if (!bImmediately) {                                                                  \
    if (timerId == -1) {/* the first one */                                             \
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": first one.");                \
      timerId = processor->SetTimer(5000, boost::bind(&ContactManager::func, this, /*bImmediately*/true));\
    } else {                                                                            \
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": skip.");                     \
    }                                                                                   \
    return;                                                                             \
  }                                                                                     \
  if (timerId != -1) {                                                                  \
    processor->CancelTimer(timerId);                                                    \
    timerId = -1;                                                                       \
  }                                                                                     \
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": do it now!");

//this can only be called in Session init
//this is cause by limitations in the pr runtime of mutex creation
ContactManager::ContactManager() :
        m_mutex("m_mutex", true), 
        m_quick_mutex("m_quick_mutex", true),
        m_state(CONTACTMGR_STATE_ADDRESS_BOOK_NOT_ALLOWED),
        m_bBlockedListReady( false),
        m_recommendationList(new ContactVector()),
        m_highlightedContactList(new ContactVector()),
        m_myself(new ContactImpl),
        m_1stTimeMergeNativeAddressBook(true),
        m_last_run_latest_created_time(0),
        m_last_viewed_recommendation_timestamp(0),
        m_recommendationHash(0),
        m_new_recommendation_count(0),
        m_app_state_mgr(NULL),
        m_foregrounded_handler_id(0),
        m_backgrounded_handler_id(0),
        m_need_to_reload_address_book(false),
        m_ContactResolveFinished(true), 
        m_seq_number(0), 
        m_1stTimeAddressbookAndCachedLoaded(false),
        m_1stTimeContactFilterSucceeded(false),
        m_notifyHideBlockTimerId(-1),
        m_addressBookSyncTimer(-1), 
        m_contactResolveTimer(-1),
        m_blockListReadyTimer(-1),
        m_notifyCallbackTimerId(-1),
        m_addressBookSyncWaitTimerId(-1),
        m_contactFilterOneRoundInCallRetryTimerId(-1),
        m_FgDeferContactUpdateTimerId(-1),
        m_contactstore_thread(GET_ADDR_BOOK_LOADING_THREAD),
        m_updateContactResponseCallback(NULL),
        m_nextCallbackId(0),
        m_bForceNotify(false),
        m_nextContactUpdateSeq(0),
        m_isStopping(false),
        m_addressBookContactsOverallHash(0),
        m_addressBookHashBasedOnWhichContactsAreFilteredLastTime(0),
        m_addressBookHashBasedOnWhichContactsAreFilteredForCurrentCycle(0),
        m_lastContactFilterTime(0),
        m_countrycodeBasedOnWhichContactsAreFilteredLastTime(""),
        m_countrycodeBasedOnWhichContactsAreFilteredForCurrentCycle(""),
        m_contactfiltering_acme_id(""),
        m_bInviteTimeLoaded(false),
        m_sort_order(sgiggle::corefacade::contacts::CONTACT_ORDER_UNDEFINED), 
        m_notified_sort_order(sgiggle::corefacade::contacts::CONTACT_ORDER_UNDEFINED),
        m_display_order(sgiggle::corefacade::contacts::CONTACT_ORDER_UNDEFINED),
        m_notified_display_order(sgiggle::corefacade::contacts::CONTACT_ORDER_UNDEFINED),
        m_config_contact_higlight_max(0),
        m_config_contact_higlight_period(0),
        m_config_contact_recommendation_max(0),
        m_config_contact_recommendation_period(0),
        m_bPstnRuleSet(false),
        m_bPstnRuleSetNotify(false)
{
  resetContactFilterSessionId_();
}

void ContactManager::start(tango::app_state_mgr_interface* app_state)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);

  m_app_state_mgr = app_state;
  m_foregrounded_handler_id = app_state->register_foregrounded_handler(boost::bind(&this_type::onAppForeground, this));
  m_backgrounded_handler_id = app_state->register_backgrounded_handler(boost::bind(&this_type::onAppBackground, this));

  m_addressBookContactsOverallHash = sgiggle::xmpp::UserInfo::getInstance()->addressBookContactsOverallHash();
  m_addressBookHashBasedOnWhichContactsAreFilteredLastTime = sgiggle::xmpp::UserInfo::getInstance()->addressBookHashBasedOnWhichContactsAreFilteredLastTime();
  m_countrycodeBasedOnWhichContactsAreFilteredLastTime = sgiggle::xmpp::UserInfo::getInstance()->userCountryCodeBasedOnWhichContactsAreFilteredLastTime();
  m_lastContactFilterTime = sgiggle::xmpp::UserInfo::getInstance()->lastContactFilterTime();
  
  m_last_viewed_recommendation_timestamp = sgiggle::xmpp::UserSettings::getInstance()->get_setting<uint64_t>(CONTACT_LAST_VIEWED_RECOMMENDATION_TIMESTAMP, 0);
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", m_last_viewed_recommendation_timestamp=" << m_last_viewed_recommendation_timestamp);
  
  m_last_run_latest_created_time = sgiggle::xmpp::UserSettings::getInstance()->get_setting<uint64_t>(LAST_RUN_LATEST_CREATED_TIME, INVALID_LATEST_CREATED_TIME);
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", m_last_run_latest_created_time=" << m_last_run_latest_created_time);

  tango::acme::Acme::getInstance()->registerHandler(FACILITATOR_ACME_SERVICE_NAME, CONTACTFILTERING_ACME_SERVICE_ID, boost::bind(&ContactManager::onAcmeContactFilterMessage, this, _1, _2));
  tango::acme::Acme::getInstance()->registerHandler(FACILITATOR_ACME_SERVICE_NAME, GETTANGOCONTACTS_ACME_SERVICE_ID, boost::bind(&ContactManager::onAcmeGetTangoContactsMessage, this, _1, _2));

  setState_(CONTACTMGR_STATE_ADDRESS_BOOK_NOT_ALLOWED);

  reload_config_settings();

  if (store_.clearContactOrderSettings()) {
    m_sort_order = store_.isSortByFirstName() ? sgiggle::corefacade::contacts::CONTACT_ORDER_FIRST_NAME : sgiggle::corefacade::contacts::CONTACT_ORDER_LAST_NAME;
    m_display_order = store_.isDisplayOrderFirstNameFirst() ? sgiggle::corefacade::contacts::CONTACT_ORDER_FIRST_NAME : sgiggle::corefacade::contacts::CONTACT_ORDER_LAST_NAME;
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", sort_order=" << m_sort_order << ", display_order=" << m_display_order);
  }

  m_blockListReadyTimer = sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor()->SetTimer(
    WAIT_BLOCKED_LIST_TIMEOUT_IN_MSEC, boost::bind(&ContactManager::setBlockedListReady, this, false));

  start_();
}

void ContactManager::start_()
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), start_);

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);

  loadCachedContacts_();

  if (sgiggle::xmpp::UserInfo::getInstance()->allowedAccessToAddressBook()) {
    allowAddressBookAccess(true);
  }
}

void ContactManager::stop()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);

  if (m_app_state_mgr) {
    m_app_state_mgr->remove_foregrounded_handler(m_foregrounded_handler_id);
    m_app_state_mgr->remove_backgrounded_handler(m_backgrounded_handler_id);
  }

  m_isStopping = true;

  for (ContactUpdateBatchMap::iterator iter = m_contactUpdateBatchMap.begin(); iter != m_contactUpdateBatchMap.end(); iter++) {
    sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessorImpl()->CancelTimer(iter->second.timer_id);
    iter->second.new_contact_request->cancel();
    iter->second.new_contact_request.reset();
  }
  m_contactUpdateBatchMap.clear();

  if(m_contact_request) {
    m_contact_request->cancel();
    m_contact_request.reset();
  }

  sgiggle::xmpp::ProcessorImpl::pointer processor = sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessorImpl();
  if (processor) {
    if (m_addressBookSyncWaitTimerId != -1) {
      processor->CancelTimer(m_addressBookSyncWaitTimerId);
      m_addressBookSyncWaitTimerId = -1;
    }
    if (m_notifyCallbackTimerId != -1) {
      processor->CancelTimer(m_notifyCallbackTimerId);
      m_notifyCallbackTimerId = -1;
    }
    if (m_notifyHideBlockTimerId != -1) {
      processor->CancelTimer(m_notifyHideBlockTimerId);
      m_notifyHideBlockTimerId = -1;
    }
    if (m_contactFilterOneRoundInCallRetryTimerId != -1) {
      processor->CancelTimer(m_contactFilterOneRoundInCallRetryTimerId);
      m_contactFilterOneRoundInCallRetryTimerId = -1;
    }
    if (m_blockListReadyTimer != -1) {
      processor->CancelTimer(m_blockListReadyTimer);
      m_blockListReadyTimer = -1;
    }
    if (m_addressBookSyncTimer != -1)  {
      processor->CancelTimer(m_addressBookSyncTimer);
      m_addressBookSyncTimer = -1;
    }
    if (m_contactResolveTimer != -1) {
      processor->CancelTimer(m_contactResolveTimer);
      m_contactResolveTimer = -1;
    }
    if (m_FgDeferContactUpdateTimerId != -1) {
      processor->CancelTimer(m_FgDeferContactUpdateTimerId);
      m_FgDeferContactUpdateTimerId = -1;
    }
  }

  setContactLoadingEnabled(false);
  stopContactReloading();

  tango::acme::Acme::getInstance()->unregisterHandler(FACILITATOR_ACME_SERVICE_NAME, CONTACTFILTERING_ACME_SERVICE_ID);
  tango::acme::Acme::getInstance()->unregisterHandler(FACILITATOR_ACME_SERVICE_NAME, GETTANGOCONTACTS_ACME_SERVICE_ID);

  clearAllCallbacks();
  setState_(CONTACTMGR_STATE_ADDRESS_BOOK_NOT_ALLOWED);
}


/*
 * called in stop() and ~ContactManager()
 */
void ContactManager::stopContactReloading()
{
  // sync wait for working thread to ensure all requests are done processing
  Waiter::pointer waiter = Waiter::pointer(new Waiter());
  if (m_contactstore_thread->async_post(boost::bind(&Waiter::notify, waiter)))
  {
    waiter->wait(10 * 1000);
  }
}

void ContactManager::event_receive_server_owned_config_updated()
{
  reload_config_settings();
}

void ContactManager::reload_config_settings()
{
  m_config_contact_higlight_max = sgiggle::config::GlobalConfig::getInstance()->getInt32(CONTACT_HIGHLIGHT_MAX,
                                                                                         server_owned_config::ServerOwnedConfigManager::getInstance()->get_int(CONTACT_HIGHLIGHT_MAX, CONTACT_HIGHLIGHT_MAX_DEFAULT));
  m_config_contact_higlight_period = sgiggle::config::GlobalConfig::getInstance()->getInt32(CONTACT_HIGHLIGHT_PERIOD,
                                                                                            server_owned_config::ServerOwnedConfigManager::getInstance()->get_int(CONTACT_HIGHLIGHT_PERIOD, CONTACT_HIGHLIGHT_PERIOD_DEFAULT)) * 1000;
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", m_config_contact_higlight_period=" << m_config_contact_higlight_period);
  
  m_config_contact_recommendation_max = sgiggle::config::GlobalConfig::getInstance()->getInt32(CONTACT_RECOMMENDATION_MAX,
                                                                                               server_owned_config::ServerOwnedConfigManager::getInstance()->get_int(CONTACT_RECOMMENDATION_MAX, CONTACT_RECOMMENDATION_MAX_DEFAULT));
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", m_config_contact_recommendation_max=" << m_config_contact_recommendation_max);
  
  m_config_contact_recommendation_period = sgiggle::config::GlobalConfig::getInstance()->getInt32(CONTACT_RECOMMENDATION_PERIOD,
                                                                                                  server_owned_config::ServerOwnedConfigManager::getInstance()->get_int(CONTACT_RECOMMENDATION_PERIOD, CONTACT_RECOMMENDATION_PERIOD_DEFAULT)) * 1000;
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", m_config_contact_recommendation_period=" << m_config_contact_recommendation_period);
}

bool ContactManager::isUIInForeground() const
{
  return m_app_state_mgr->is_ui_in_foreground();
}

void ContactManager::onAppForeground()
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), onAppForeground);
  
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__
              << ", m_state="<< getStateName(m_state)
              << ", m_need_to_reload_address_book=" << m_need_to_reload_address_book);
  
  if (store_.clearContactOrderSettings()) {
    setSortAndDisplayOrder((store_.isSortByFirstName() ? sgiggle::corefacade::contacts::CONTACT_ORDER_FIRST_NAME : sgiggle::corefacade::contacts::CONTACT_ORDER_LAST_NAME),
                           (store_.isDisplayOrderFirstNameFirst() ? sgiggle::corefacade::contacts::CONTACT_ORDER_FIRST_NAME : sgiggle::corefacade::contacts::CONTACT_ORDER_LAST_NAME));
  }
  
  if (m_need_to_reload_address_book) {
    setNeedToLoadAddressBook();
  } else {
    // NB: setTimer since when app-foreground the CPU is too hot, so wait 3s
    m_FgDeferContactUpdateTimerId
      = xmpp::MediaEngineManager::getInstance()->getProcessor()->SetTimer(3500, boost::bind(&this_type::setNeedToContactUpdate, this, boost::optional<com::tango::facilitator::client::proto::v3::filteraccount::ContactFilteringCauseEnum>())); 
  }
}

void ContactManager::onAppBackground()
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), onAppBackground);
  
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);
  saveTimeToLocalStorage_();
}

/**
 * called when user grants access of the address book
 */
void ContactManager::allowAddressBookAccess(bool bAllow)
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD2(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), allowAddressBookAccess, bAllow);

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", bAllow=" << bAllow << ", state=" << getStateName(m_state));

  sgiggle::xmpp::UserInfo::getInstance()->set_allowAccessToAddressBook(bAllow);

  {
    pr::scoped_lock lock(m_mutex);
    if (bAllow) {
      if (m_state == CONTACTMGR_STATE_ADDRESS_BOOK_NOT_ALLOWED) {
        setState_(CONTACTMGR_STATE_ADDRESS_BOOK_NOT_LOADED);
        setNeedToLoadAddressBook();
      }
      return;
    }
    // NOT ALLOW ACCESS ADDRESS BOOK
    m_1stTimeAddressbookAndCachedLoaded = true;
    if (m_state == CONTACTMGR_STATE_LOADING_ADDRESS_BOOK) {
      // waiting for loading finished. then it will set state to NOT_ALLOWED
      return;
    }

    if (m_state == CONTACTMGR_STATE_CONTACT_FILTERING) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", cancel current contact filtering");
      cancelContactResolve();
    }

    setState_(CONTACTMGR_STATE_ADDRESS_BOOK_NOT_ALLOWED);
    removeAddressBookContacts_();
    setNeedToContactUpdate();
  }
  notifyServiceCallbacks_();
}

void ContactManager::onAddressBookChanged()
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), onAddressBookChanged);

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ );

  setNeedToLoadAddressBook();
}

void ContactManager::checkLoadAddressBook()
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), checkLoadAddressBook);

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", m_need_to_reload_address_book=" << m_need_to_reload_address_book);

  if (m_need_to_reload_address_book) {
    setNeedToLoadAddressBook();
  }
}

void ContactManager::setNeedToLoadAddressBook()
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), setNeedToLoadAddressBook);

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", state=" << getStateName(m_state) << ", UIInForeground=" << isUIInForeground());
  m_need_to_reload_address_book = true;

  if (isUIInForeground()) {
    { // for atomicity
      pr::scoped_lock lock(m_mutex);
      if (m_state == CONTACTMGR_STATE_CONTACT_FILTERING) {
        SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", cancel current contact filtering");
        cancelContactResolve();
      }
      
      if (m_state == CONTACTMGR_STATE_ADDRESS_BOOK_LOADED || 
          m_state == CONTACTMGR_STATE_ADDRESS_BOOK_NOT_LOADED) {
        
        m_need_to_reload_address_book = false;
        setState_(CONTACTMGR_STATE_LOADING_ADDRESS_BOOK);
        m_contactstore_thread->async_post(boost::bind(&ContactManager::loadContactsThreadProc, this));
        
      }
    } // lock scope
  } // if
}

// This runs in contactstore thread.
void ContactManager::loadContactsThreadProc()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);
  
  ContactImplMap addressBookContacts;
  if (!store_.load(addressBookContacts)) {
    SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ", load address book failed");
    { // for atomicity
      pr::scoped_lock guard(m_mutex);
      setState_(CONTACTMGR_STATE_ADDRESS_BOOK_NOT_LOADED);
    }
    return;
  }
  
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD2(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), mergeOrPurgeLoadedContacts, addressBookContacts);
}

void ContactManager::mergeOrPurgeLoadedContacts(ContactImplMap addressBookContacts)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", sz=" << addressBookContacts.size());
  bool bAddressBookChanged = false;
  bool bContactFilterChanged = false;
  {
    pr::scoped_lock lock(m_mutex);
    
    xmpp::UserInfo* userinfo = xmpp::UserInfo::getInstance();
    sgiggle::corefacade::contacts::PhoneNumber my_phonenumber(userinfo->countrycodenumber(), sgiggle::phone_formatter::removeFormat(userinfo->subscribernumber()), sgiggle::corefacade::contacts::PHONE_TYPE_GENERIC);
    std::string userEmail = userinfo->email();
    sgiggle::trim(userEmail);
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__
                << ", my_phonenumber=" << userinfo->countrycodenumber() << "-" << userinfo->subscribernumber()
                << ", my_email=" << userEmail);
    uint32_t overallHash = 0;
    
    int64_t latest_created_time = m_last_run_latest_created_time;
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__
                << ", m_last_run_latest_created_time=" << m_last_run_latest_created_time
                << ", m_1stTimeMergeNativeAddressBook=" << m_1stTimeMergeNativeAddressBook);
    
    // added/changed from native address book
    foreach (newIter, addressBookContacts) {
      if (m_isStopping)
        return;
      
      SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", hash=" << newIter->second->getHash()
                  << ", lastName=" << newIter->second->getLastName()
                  << ", first_name=" << newIter->second->getFirstName()
                  << ", deviceContactId=" << newIter->second->getDeviceContactId()
                  << ", sid=" << newIter->second->getSid()
                  << ", createdTime=" << newIter->second->getCreatedTime()
                  << ", lastModifiedTime=" << newIter->second->getLastModifiedTime());
      
      SG_ASSERT(newIter->second->getAccountId().empty());
      
      // exclude myself
      if (newIter->second->getPhoneNumberSize() == 1 && newIter->second->getPhoneNumberAt(0)->compare(my_phonenumber))
        continue;
      if (!userEmail.empty() && newIter->second->getEmailSize() == 1 && newIter->second->getEmailAt(0) == userEmail)
        continue;
      
      SG_ASSERT(!newIter->second->getCompoundId().empty());
      ContactImplMap::iterator oldIter = m_addressBookContacts.find(newIter->first);
      
      overallHash ^= newIter->second->getHashUint32();
      
      if (oldIter != m_addressBookContacts.end()) {
        if (!oldIter->second->isDeleted()){
          if (!oldIter->second->isFromAddressbook()) {
            SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", name=" << oldIter->second->getDisplayName() << ", setFromAddressbook");
            oldIter->second->setFromAddressbook(true);
            oldIter->second->setContactChanged();
            bAddressBookChanged = true;
          }
          if (newIter->second->getDisplayName() != oldIter->second->getDisplayName()) {
            SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", hash=" << newIter->second->getHash() << ", change displayName, " << oldIter->second->getDisplayName() << "->" << newIter->second->getDisplayName());
            oldIter->second->setDisplayName(newIter->second->getDisplayName());
            oldIter->second->setContactChanged();
            bAddressBookChanged = true;
          }
          if (newIter->second->getDeviceContactId() != oldIter->second->getDeviceContactId()
              || newIter->second->hasNativePicture() != oldIter->second->hasNativePicture()) {
            SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", name=" << oldIter->second->getDisplayName()
                        << ", change deviceContactId, " << oldIter->second->getDeviceContactId() << "->"<< newIter->second->getDeviceContactId()
                        << ", change hasPicture=" << newIter->second->hasNativePicture());
            oldIter->second->setHasNativePicture(newIter->second->hasNativePicture());
            oldIter->second->setDeviceContactId(newIter->second->getDeviceContactId());
            oldIter->second->setContactChanged();
            bAddressBookChanged = true;
          }
          if (newIter->second->getCompoundId() != oldIter->second->getCompoundId()) {
            SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", name=" << oldIter->second->getDisplayName()
                        << ", change compoundId, " << oldIter->second->getCompoundId() << "->" << newIter->second->getCompoundId());
            oldIter->second->setCompoundId(newIter->second->getCompoundId());
            oldIter->second->setContactChanged();
            bAddressBookChanged = true;
          }
          /*if (newIter->second->getCreatedTime() != oldIter->second->getCreatedTime()) {
           SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", name=" << oldIter->second->getDisplayName()
           << ", change lastModifiedTime, " << oldIter->second->getCreatedTime() << "->" << newIter->second->getCreatedTime());
           oldIter->second->setCreatedTime(newIter->second->getCreatedTime());
           oldIter->second->setContactChanged();
           bAddressBookChanged = true;
           }*/
          if (newIter->second->getLastModifiedTime() != oldIter->second->getLastModifiedTime()) {
            SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", name=" << oldIter->second->getDisplayName()
                        << ", change lastModifiedTime, " << oldIter->second->getLastModifiedTime() << "->" << newIter->second->getLastModifiedTime());
            oldIter->second->setLastModifiedTime(newIter->second->getLastModifiedTime());
            oldIter->second->setContactChanged();
            bAddressBookChanged = true;
          }
          if (newIter->second->getPhoneNumberSize() > 0 && oldIter->second->getPhoneNumberSize() > 0 && newIter->second->getPhoneNumberAt(0)->phoneType != oldIter->second->getPhoneNumberAt(0)->phoneType) {
            SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", name=" << oldIter->second->getDisplayName()
                        << ", change phoneType, " << oldIter->second->getPhoneNumberAt(0)->phoneType << "->" << newIter->second->getPhoneNumberAt(0)->phoneType);
            SG_ASSERT(newIter->second->getPhoneNumberSize() == 1);
            SG_ASSERT(oldIter->second->getPhoneNumberSize() == 1);
            oldIter->second->removeAllPhoneNumbers();
            sgiggle::corefacade::contacts::PhoneNumberPointer pnp = newIter->second->getPhoneNumberAt(0);
            oldIter->second->addPhoneNumber(PhoneNumber(*pnp));
            oldIter->second->setContactChanged();
            bAddressBookChanged = true;
          }
          continue;
        }
      }
      
      ContactImplPointer contactImpl = boost::make_shared<ContactImpl>(newIter->second);
      
      // always update the latest_created_time to the 'latest'
      if(contactImpl->getCreatedTime() > latest_created_time) {
        latest_created_time = contactImpl->getCreatedTime();
      }
      
      // Adjust the 'createdTime' for a Contact:
      // - for the first run
      //    - if for a fresh install/upgrade, set everyone as 0
      //    # NB: I was to use UserInfo::isFirstRunAfterFreshInstall but it can't handle upgrade
      //    - otherwise, set newer than last_run as 'now' and others as 0
      // - otherwise: set as 'now'
      if(m_1stTimeMergeNativeAddressBook) {
        if(m_last_run_latest_created_time == INVALID_LATEST_CREATED_TIME) {
          contactImpl->setCreatedTime(0);
        } else {
          if((uint64_t)(contactImpl->getCreatedTime()) > m_last_run_latest_created_time) {
            contactImpl->setCreatedTime();
          } else {
            contactImpl->setCreatedTime(0);
          }
        }
      } else {
        contactImpl->setCreatedTime();
      }
      
      if(oldIter != m_addressBookContacts.end()) { // if was deleted, then replace
        SG_ASSERT(oldIter->second->isDeleted());
        SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", " << oldIter->second->getDisplayName() << ", to be replaced.");
        oldIter->second = contactImpl;
      } else {
        m_addressBookContacts[contactImpl->getHash()] = contactImpl;
      }
      contactImpl->setFromAddressbook(true);
      contactImpl->setContactChanged();
      
      SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", add contact, hash=" << contactImpl->getHash()
                  << ", disp_name=" << contactImpl->getDisplayName()
                  << ", createdTime=" << contactImpl->getCreatedTime());
      
      bAddressBookChanged = true;
    } // for
    
    // deleted from native-address-book
    ContactImplMap social_friend_map;
    foreach (oldIter, m_addressBookContacts) {
      if (m_isStopping) {
        return;
      }
      
      if (oldIter->second->isDeleted() || !oldIter->second->isFromAddressbook()) {
        continue;
      }
      
      ContactImplMap::iterator newIter = addressBookContacts.find(oldIter->first);
      if (newIter == addressBookContacts.end() && !oldIter->second->isUnderHashMigration__()) {
        asNativeAddrBookAndNewSocial_(oldIter->second, social_friend_map);
        // also not from native addressbook any more
        oldIter->second->setDeleted(true);
        oldIter->second->setContactChanged();
        bAddressBookChanged = true;
      }
    }
    newSocialContacts_(social_friend_map);
    
    // NB: update and save on disk
    m_last_run_latest_created_time = latest_created_time;
    sgiggle::xmpp::UserSettings::getInstance()->set_setting<uint64_t>(LAST_RUN_LATEST_CREATED_TIME, m_last_run_latest_created_time);
    m_addressBookContactsOverallHash = (uint32_t)overallHash;
    sgiggle::xmpp::UserInfo::getInstance()->setAddressBookContactsOverallHash(m_addressBookContactsOverallHash);
    sgiggle::xmpp::UserInfo::getInstance()->save();
    
    // update status
    m_1stTimeMergeNativeAddressBook = false;
    m_1stTimeAddressbookAndCachedLoaded = true;
    
    // post-process
    bContactFilterChanged = processLoadedContacts__();
    SGLOG_DEBUG ("ContactManager::" <<__FUNCTION__ 
        << "Address book size is " << m_addressBookContacts.size()
        << ", overallhash=" << m_addressBookContactsOverallHash
        << ", lastCFHash=" << m_addressBookHashBasedOnWhichContactsAreFilteredLastTime);
  }// lock scope
  
  // will notify when the filter succeeded, otherwise there're too many callbacks.
  if (bContactFilterChanged || bAddressBookChanged) {
    setNeedToContactUpdate();
  } else if (calculatePstnCallValue()) {
    m_1stTimeContactFilterSucceeded = true;
    notifyServiceCallbacks_(bAddressBookChanged);
  }
}

// m_mutex
bool ContactManager::processLoadedContacts__()
{
  SGLOG_DEBUG("ContactManager::"<<__FUNCTION__);
  
  if (!xmpp::UserInfo::getInstance()->allowedAccessToAddressBook()) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": address book access was disabled during loading.");
    setState_(CONTACTMGR_STATE_ADDRESS_BOOK_NOT_ALLOWED);
    removeAddressBookContacts_();
    return true;
  }
  
  bool bRet = false;
  if (m_state == CONTACTMGR_STATE_CONTACT_FILTERING) {
    // I don't see need to cancel.
    // SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", cancel current contact filtering");
    // cancelContactResolve();
    bRet = true;
  }
  loadCachedTime_();
  setState_(CONTACTMGR_STATE_ADDRESS_BOOK_LOADED);
  
  return bRet;
}

/*
 * called in ContactService when mobile device detects that contact display or
 * sort order is changed
 */
void ContactManager::setSortAndDisplayOrder(const sgiggle::corefacade::contacts::ContactOrderEnum sort_order, const sgiggle::corefacade::contacts::ContactOrderEnum display_order)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", sort_order=" << m_sort_order << " -> " << sort_order
              << ", display_order=" << m_display_order << " -> " << display_order);

  if (m_display_order != display_order) {
    m_display_order = display_order;
    m_sort_order = sort_order;
    setNeedToLoadAddressBook();
    return;
  }

  if (m_sort_order != sort_order) {
    m_sort_order = sort_order;
    notifyServiceCallbacks_();
  }
}

// change contactImpl according to social contact \p c
// can modify two info: name and avatar. and name cannot be modified if contact is from address book.
// sync: contactImpl
bool ContactManager::adaptSocialContact_(/*to*/ContactImplPointer contactImpl, /*from*/const Contact& c)
{
  bool bChanged = false;

  if (!contactImpl->isFromAddressbook() && contactImpl->getDisplayName() != c.getDisplayName()) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", set displayName to " << c.getDisplayName() << ", accountId=" << c.getAccountId());
    contactImpl->setFirstName(c.getFirstName());
    contactImpl->setLastName(c.getLastName());
    contactImpl->setDisplayName(c.getDisplayName());
    contactImpl->setContactChanged();
    bChanged = true;
  }
  if (contactImpl->getThumbnailUrl() != c.getThumbnailUrl()) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", set thumbnailUrl to " << c.getThumbnailUrl() << ", accountId=" << c.getAccountId());
    contactImpl->setThumbnailUrl(c.getThumbnailUrl());
    contactImpl->setThumbnailPath("");
    contactImpl->setContactChanged();
    bChanged = true;
  }
  if (!c.getThumbnailPathDirect().empty() && contactImpl->getThumbnailPathDirect() != c.getThumbnailPathDirect()) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", set thumbnailPath to " << c.getThumbnailPathDirect() << ", accountId=" << c.getAccountId());
    contactImpl->setThumbnailPath(c.getThumbnailPathDirect());
    contactImpl->setContactChanged();
    bChanged = true;
  }

  return bChanged;
}

// sync: contactImpl
bool ContactManager::stripTangoContact_(ContactImplPointer contactImpl)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << contactImpl->getDisplayName()
              << "(" << contactImpl->getHash() << ", " << contactImpl->getAccountId() << ")");
  
  if (contactImpl->getAccountId().empty()) {
    SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ": empty account id. "
                << contactImpl->getDisplayName() << "(" << contactImpl->getHash() << ")");
    return false;
  }
  
  ContactImplMultiMap::iterator itTango = m_tangoContacts.find(contactImpl->getAccountId());
  if (itTango == m_tangoContacts.end()) {
    SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ": is not in m_tangoContacts. "
                << contactImpl->getDisplayName() << "(" << contactImpl->getHash() << ")");
    return false;
  }
  
  foreach (itContact, itTango->second) {
    if ((*itContact)->getHash() == contactImpl->getHash()) {
      
      if((*itContact)->isFromAddressbook()) {
        // implies non-empty
        (*itContact)->setAccountId("");
        (*itContact)->setContactChanged();
      } else {
        (*itContact)->setDeleted(true);
        (*itContact)->setContactChanged();
      }
      
      itTango->second.erase(itContact);
      if (itTango->second.empty()) {
        m_tangoContacts.erase(itTango);
      }
      
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": stripped: " << contactImpl->getDisplayName() << "(" << contactImpl->getHash() << ")");
      return true;
    }
  }
  
  return false;
}

// the contacts with the \p accountId will not be a social friend.
// if it's address-book friend, we still keep it.
bool ContactManager::stripSocialContactsByAccountId(const std::string& accountId)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", accountId=" << accountId);
  SG_ASSERT(!isAccountBlocked(accountId) && !isAccountHidden(accountId));

  pr::scoped_lock lock(m_mutex);
  
  ContactImplMultiMap::iterator it_tango = m_tangoContacts.find(accountId);
  if (it_tango == m_tangoContacts.end()) {
    SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ", accountId not found in tango contacts. accountId=" << accountId);
    return false;
  }

  LET(iter_ahead, it_tango->second.begin());
  LET(itVector, it_tango->second.begin());
  for (; iter_ahead != it_tango->second.end(); ++iter_ahead)
  {
    if ((*iter_ahead)->isFromAddressbook()) {
      // set non-social
      if((*iter_ahead)->isFromSocial()) {
        (*iter_ahead)->setFromSocial(false);
        (*iter_ahead)->setContactChanged();
      }
      *(itVector++) = *iter_ahead;
    } else {
      (*iter_ahead)->setDeleted(true);
      (*iter_ahead)->setContactChanged();
    }
  }
  
  if (itVector == it_tango->second.begin() /*implies empty*/) {
    m_tangoContacts.erase(it_tango);
  } else {
    it_tango->second.erase(itVector, it_tango->second.end());
  }
  return true;
}

// create a social contact with hash as acct:accountId
// the \c is from social module, so no need to fetch
// sync: mutex
void ContactManager::newSocialContact_(ContactImplPointer contactImpl, bool bToHighlight)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": " << contactImpl->getDisplayName()
              << "(" << contactImpl->getAccountId() << "," << contactImpl->getHash() << "), bToHighlight=" << bToHighlight);
  SG_ASSERT(!contactImpl->getAccountId().empty());
  
  contactImpl->setHash(composeHashByAccountIdForSocialOnly(contactImpl->getAccountId()));
  contactImpl->setFromSocial(true);
  if (!bToHighlight) {
    contactImpl->setCreatedTime(0);
  } else {
    contactImpl->setCreatedTime();
  }
  contactImpl->setContactChanged();
  
  m_addressBookContacts[contactImpl->getHash()] = contactImpl;
  SG_ASSERT(!contains(m_tangoContacts, contactImpl->getAccountId()));
  m_tangoContacts[contactImpl->getAccountId()].push_back(contactImpl);
}

// NB: will fetch since not from 'social'
void ContactManager::newSocialContacts_(ContactImplMap& social_friend_map)
{
  BOOST_FOREACH(ContactImplMap::value_type& entry, social_friend_map) {
    if (m_isStopping) {
      return;
    }
    
    ContactImplPointer contactImpl = entry.second;
    ContactImplPointer socialContact = social::ProfileSimpleFetcher::getInstance()->getSocialContact(contactImpl->getAccountId());
    if (socialContact) {
      contactImpl->setFirstName(socialContact->getFirstName());
      contactImpl->setLastName(socialContact->getLastName());
      contactImpl->setDisplayName("");
      contactImpl->setContactChanged();
    }
    newSocialContact_(contactImpl, /*bToHighlight*/ false);
  } // foreach
}

// add by creating a new social contact or adapting the
// social contact \p c to an existing one
bool ContactManager::addSocialContactImpl(const Contact& c, bool bNew)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", bNew=" << bNew << ", accountId=" << c.getAccountId() << ", displayName=" << c.getDisplayName()
    << ", firstName=" << c.getFirstName() << ", lastName=" << c.getLastName() << ", thumbnailUrl=" << c.getThumbnailUrl());
  if (c.getAccountId().empty()) {
    SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ", accountId should not be empty. hash=" << c.getHash());
    return false;
  }

  pr::scoped_lock lock(m_mutex);

  bool bNotify = false;
  LET(tangoIt, m_tangoContacts.find(c.getAccountId()));
  if (tangoIt != m_tangoContacts.end()) {
    BOOST_FOREACH(ContactImplPointer& contactImpl, tangoIt->second) {
      if (!contactImpl->isFromSocial()) {
        contactImpl->setFromSocial(true);
        contactImpl->setContactChanged();
        bNotify = true;
      }
      bNotify |= adaptSocialContact_(contactImpl, c);
    }
  } else {
    newSocialContact_(ContactImplPointer(new ContactImpl(c)), /*bToHighlight*/bNew);
    bNotify = true;
  }
  return bNotify;
}

bool ContactManager::addSocialContact(const Contact& c, bool bNew)
{
  if(addSocialContactImpl(c, bNew)) {
    notifyServiceCallbacks_();
    return true;
  }
  return false;
}

void ContactManager::setCurrentSocialFriends(const std::vector<Contact>& friendList)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", sz=" << friendList.size());
  
  bool bNotify = false;
  BOOST_FOREACH(const Contact& c, friendList) {
    bNotify |= addSocialContactImpl(c, /*bToHighligh*/false);
  }
  
  if (bNotify) {
    notifyServiceCallbacks_();
  }
}

bool ContactManager::adaptSocialContactImpl(const Contact& c)
{
  SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ": displayName=" << c.getDisplayName() << ", accountId=" << c.getAccountId());
  SG_ASSERT(!c.getAccountId().empty());

  pr::scoped_lock lock(m_mutex);

  // can modify two info: name and avatar. and name cannot be modified if contact is from address book.
  bool bNotify = false;
  ContactImplMultiMap::iterator it_tango = m_tangoContacts.find(c.getAccountId());
  if (it_tango != m_tangoContacts.end()) {
    BOOST_FOREACH (ContactImplPointer& contactImpl, it_tango->second) {
      bNotify |= adaptSocialContact_(contactImpl, c);
    }
  }

  social::ProfileSimpleFetcher::getInstance()->modifyContact(c);
  return bNotify;
}

bool ContactManager::adaptSocialContact(const Contact& c)
{
  SGLOG_TRACE("ContactManager::" << __FUNCTION__ );
  
  if(adaptSocialContactImpl(c)) {
    notifyServiceCallbacks_();
    return true;
  }
  return false;
}

void ContactManager::adaptSocialContacts(const std::vector<Contact>& contacts)
{
  SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", sz=" << contacts.size());

  bool bNotify = false;
  BOOST_FOREACH(const Contact& c, contacts) {
    bNotify |= adaptSocialContactImpl(c);
  }

  if (bNotify) {
    notifyServiceCallbacks_();
  }
}

bool ContactManager::removeSocialContact(const std::string& accountId)
{
  SGLOG_WARN("ContactManager::" << __FUNCTION__ << ", accountId=" << accountId);

  if (stripSocialContactsByAccountId(accountId)) {
    notifyServiceCallbacks_();
    return true;
  }
  return false;
}

// pre: m_mutex
void ContactManager::removeAddressBookContacts_()
{
  // remove all fromAddressbook-only contacts. for contact that both isFromAddressbook and
  // isFromSocial as true, we strip isFromAddressbook flag only.
  LET(itTango, m_tangoContacts.begin());
  while (itTango != m_tangoContacts.end()) {
    LET(itTango2, itTango->second.begin());
    while (itTango2 != itTango->second.end()) {
      if (!(*itTango2)->isFromAddressbook()) {
        itTango2++;
        continue;
      }
      if ((*itTango2)->isFromSocial()) {
        (*itTango2)->setFromAddressbook(false);
        (*itTango2)->setContactChanged();
        itTango2++;
        continue;
      }
      m_addressBookContacts.erase((*itTango2)->getHash());
      itTango2 = itTango->second.erase(itTango2);
    } // while
    if (itTango->second.empty()) {
      itTango = m_tangoContacts.erase(itTango);
    } else {
      itTango++;
    }
  } // while
  
  // now, if from-address-book, then it's address-book-only.
  LET(iterAB, m_addressBookContacts.begin());
  for(; iterAB != m_addressBookContacts.end();) {
    if(iterAB->second->isFromAddressbook()) {
      iterAB = m_addressBookContacts.erase(iterAB);
    } else {
      ++iterAB;
    }
  }
  
  m_addressBookContactsOverallHash = 0;
}

// strip "social"
// if \p contactImpl is social friend, then save it's in \p social_friend_map
// sync: protect contactImpl
bool ContactManager::asNativeAddrBookAndNewSocial_(ContactImplPointer contactImpl, ContactImplMap& social_friend_map)
{
  SGLOG_TRACE("ContactManager::" << __FUNCTION__  << ": "
              << contactImpl->getDisplayName() << "(" << contactImpl->getHash() << ", " << contactImpl->getSid() << ")");
  
  const std::map<std::string, std::string> emptyCapabilities;
  if(!contactImpl->getAccountId().empty()) {
    unblockTangoContact(contactImpl->getAccountId());
    unhideTangoContact(contactImpl->getAccountId());
    
    if (contactImpl->isFromSocial()) {
      SGLOG_TRACE("ContactManager::" << __FUNCTION__  << ": "
                  << contactImpl->getDisplayName() << "(" << contactImpl->getHash() << "), keep as social.");
      ContactImplPointer contactImplNew = ContactImplPointer(new ContactImpl(contactImpl));
      social_friend_map[contactImplNew->getAccountId()] = contactImplNew;
      contactImplNew->setFromAddressbook(false);
      contactImplNew->setCapabilities(emptyCapabilities);
      contactImplNew->setContactChanged();
    }
    
    stripTangoContact_(contactImpl);
    return true;
  }
  return false;
}

// sync: contactImpl, tangoContacts
void ContactManager::handleSocialMerge_(ContactImplPointer contactImpl,
                                        ContactImplMap* pAddressBookContacts,
                                        ContactImplMultiMap* pTangoContacts)
{
  if(!pAddressBookContacts) {
    pAddressBookContacts =  &m_addressBookContacts;
  }
  if(!pTangoContacts) {
    pTangoContacts =  &m_tangoContacts;
  }
  LETREF(addressBookContacts, *pAddressBookContacts);
  LETREF(tangoContacts, *pTangoContacts);
  
  if (contains(tangoContacts, contactImpl->getAccountId())) {
    LETCREF(contactV, tangoContacts[contactImpl->getAccountId()]);
    if (contactV[0]->isFromSocial()) {
      contactImpl->setFromSocial(true);
      contactImpl->setThumbnailUrl(contactV[0]->getThumbnailUrl());
      contactImpl->setThumbnailPath("");
    }
    if (!contactV[0]->isFromAddressbook()) {
      SG_ASSERT(contactV.size() == 1 && isSocialOnly(contactV[0]->getHash()));
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", remove social contact "
                  << contactV[0]->getDisplayName() << "(" << contactV[0]->getHash() << ").");
      // This was a remove!!!!! but it's safe with contact-filter since currentIt will never point
      // to the contact not-from-address-book.
      addressBookContacts.erase(contactV[0]->getHash());
      tangoContacts.erase(contactImpl->getAccountId());
    }
  }
}

// sync: contactImpl, tangoContacts
bool ContactManager::addToTangoContacts_(ContactImplPointer contactImpl, ContactImplMultiMap* pTangoContacts)
{
  if(!pTangoContacts) {
    pTangoContacts =  &m_tangoContacts;
  }
  LETREF(tangoContacts, *pTangoContacts);
  
  if (contactImpl->getAccountId().empty()) {
    SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ": empty account id. "
                << contactImpl->getDisplayName() << "(" << contactImpl->getHash() << ")");
    return false;
  }
  
  std::vector<ContactImplPointer> & contactV(tangoContacts[contactImpl->getAccountId()]);
  foreach (itTangoContact, contactV) {
    if ((*itTangoContact)->getHash() == contactImpl->getHash()) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", contact already exists!"
                  << contactImpl->getDisplayName() << "(" << contactImpl->getHash() << ")");
      return true;
    }
  }
  
  contactV.insert(contactV.end(), contactImpl);
  SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", " << contactImpl->getDisplayName() << "(" << contactImpl->getHash() << ")");
  return true;
}

void ContactManager::addTangoContactFromContactFilter_(ContactImplPointer contactImpl,
                                                       const std::string& accountId,
                                                       const std::map<std::string, std::string>& capabilities)
{
  contactImpl->setAccountId(accountId);
  contactImpl->setCapabilities(capabilities);
  contactImpl->setContactChanged();
  handleSocialMerge_(contactImpl);
  addToTangoContacts_(contactImpl);
}

// promote address-book contact with the hash
// sync: mutex.lock()
bool ContactManager::addTangoContactFromContactFilter_(const std::string& hash,
                                                       const std::string& accountId,
                                                       const std::map<std::string, std::string>& capabilities,
                                                       ContactImplMap& social_friend_map)
{
  if (ContactImplPointer contactImpl = getAddressBookContactByHash_(hash)) {
    if(!contactImpl->isDeleted()) {
      SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ": " << contactImpl->getDisplayName() << "(" << hash << "), set: "
                  << accountId << ", " << printable_container(capabilities));
      
      if(!contactImpl->getAccountId().empty()) {
        if(contactImpl->getAccountId() != accountId) {
          asNativeAddrBookAndNewSocial_(contactImpl, social_friend_map);
          addTangoContactFromContactFilter_(contactImpl, accountId, capabilities);
          return true;
        } else if(!contactImpl->areCapabilitiesSameAs(capabilities)) {
          contactImpl->setCapabilities(capabilities);
          contactImpl->setContactChanged();
          return true;
        }
      } else {
        addTangoContactFromContactFilter_(contactImpl, accountId, capabilities);
        return true;
      }
    }
  }

  SGLOG_DEBUG(__FUNCTION__ << ": " << accountId << "(" << hash << "), can't found or deleted locally.");
  return false;
}

bool ContactManager::isAccountBlocked(const std::string& accountId) const
{
  pr::scoped_rlock lock(m_accountIds_rwlock.rmutex());
  return contains(m_blockedAccountIds, accountId);
}

bool ContactManager::isAccountHidden(const std::string& accountId) const
{
  pr::scoped_rlock lock(m_accountIds_rwlock.rmutex());
  return contains(m_hiddenAccountIds, accountId);
}

// block a tango contact added before. This account won't be shown in getContacts()
bool ContactManager::blockTangoContact(const std::string& accountId)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", accountId=" << accountId);

  {
    pr::scoped_wlock lock(m_accountIds_rwlock.wmutex());

    if (contains(m_blockedAccountIds, accountId)) {
      SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ", already blocked. accountId=" << accountId);
      return false;
    }
    m_blockedAccountIds.insert(accountId);
  }
  
  notifyHideBlock_();
  
  return true;
}

// unblock a tango contact blocked before.
bool ContactManager::unblockTangoContact(const std::string& accountId)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", accountId=" << accountId);

  {
    pr::scoped_wlock lock(m_accountIds_rwlock.wmutex());

    std::set<std::string>::iterator it = m_blockedAccountIds.find(accountId);
    if (it == m_blockedAccountIds.end()) {
      SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ", not blocked, accountId=" << accountId);
      return false;
    }
    m_blockedAccountIds.erase(it);
  }
  
  notifyHideBlock_();
  
  return true;
}

// hide a tango contact added before. This account won't be shown in getContacts()
bool ContactManager::hideTangoContact(const std::string& accountId)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", accountId=" << accountId);

  {
    pr::scoped_wlock lock(m_accountIds_rwlock.wmutex());

    if (contains(m_hiddenAccountIds, accountId)) {
      SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ", already hidden. accountId=" << accountId);
      return false;
    }
    m_hiddenAccountIds.insert(accountId);
  }

  notifyHideBlock_();
  
  return true;
}

// unhide a tango contact blocked before.
bool ContactManager::unhideTangoContact(const std::string& accountId)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", accountId=" << accountId);

  {
    pr::scoped_wlock lock(m_accountIds_rwlock.wmutex());

    std::set<std::string>::iterator it = m_hiddenAccountIds.find(accountId);
    if (it == m_hiddenAccountIds.end()) {
      SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ", not hidden, accountId=" << accountId);
      return false;
    }
    m_hiddenAccountIds.erase(it);
  }

  notifyHideBlock_();
  
  return true;
}

// Normally, the user won't do hide/unhide/block/unblock often. But at start,
// the hide/unhide/block/unblock is called too many times. Thus need to batch the
// request.
// the first one will set a timer with bImmediately=true, the others within 5s will
// see the timer and simply return. Until the timer is ready, the first one executes.
void ContactManager::notifyHideBlock_(bool bImmediately /*=false */)
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD2(DispatcherThread::get_timer_dispatcher(), notifyHideBlock_, bImmediately);

  batchedRequest(m_notifyHideBlockTimerId, notifyHideBlock_, bImmediately);
  
  sgiggle::corefacade::contacts::ContactVectorPointer empty_contacts(new sgiggle::corefacade::contacts::ContactVector);
  notifyServiceCallbacks__(/*added*/empty_contacts, /*removed*/empty_contacts, /*changed*/empty_contacts,
                           /*bOrderDisplayChanged*/false, empty_contacts, empty_contacts);
}

// social's local cache loaded so that 'BlockedListReady', notify the callbacks.
void ContactManager::setBlockedListReady(bool bFromSocial)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", bFromSocial=" << bFromSocial);

  { // atomicity
    pr::scoped_lock guard(m_quick_mutex);
    if (m_bBlockedListReady) {
      return;
    }
    m_bBlockedListReady = true;
  }

  if (m_blockListReadyTimer != -1) {
    sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor()->CancelTimer(m_blockListReadyTimer);
    m_blockListReadyTimer = -1;
  }

  sgiggle::corefacade::contacts::ContactVectorPointer added_contacts(new sgiggle::corefacade::contacts::ContactVector);
  sgiggle::corefacade::contacts::ContactVectorPointer empty_contacts(new sgiggle::corefacade::contacts::ContactVector);
 
  {
    sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
    BOOST_FOREACH(const ContactImplMap::value_type& entry, m_lastNotifyContacts) {
      if (entry.second->isDeleted() || entry.second->isNotToBeDisplayed()) {
        continue;
      }
      ContactPointer c(new Contact(entry.second));
      added_contacts->push_back(c);
    }
  }
  notifyServiceCallbacks__(added_contacts, empty_contacts, empty_contacts, /*bOrderDisplayChanged*/ true, empty_contacts, empty_contacts);
}

bool ContactManager::isBlockedListReady() const
{
  pr::scoped_lock lock(m_quick_mutex);

  return m_bBlockedListReady;
}

bool ContactManager::isAddressBookLoaded() const
{
  return m_state > CONTACTMGR_STATE_LOADING_ADDRESS_BOOK;
}

ContactVectorPointer ContactManager::getContacts(bool bTango, bool bATM, bool bPotentialTM, bool bOther, bool bUnique) const
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": bTango=" << bTango << ", bATM=" << bATM << ", bPotentialTM=" << bPotentialTM << ", bOther=" << bOther << ", bUnique=" << bUnique << ", sorted=" << m_addressBookSortedList.size());

  ContactVectorPointer allContacts(new ContactVector);

  if (!isBlockedListReady()) {
    return allContacts;
  }

  const std::string selfAccountId = getMyself().getAccountId();

  sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());

  std::set<std::string> allTangoAccounts;
  BOOST_FOREACH (const ContactImplPointer& contactImpl, m_addressBookSortedList) {
    const std::string &accountId = contactImpl->getAccountId();

    if (contactImpl->isBlocked() || contactImpl->isHidden() || contactImpl->isNotToBeDisplayed()) {
      continue;
    }

    if (!selfAccountId.empty() && accountId == selfAccountId) {
      continue;
    }

    if (bUnique && contains(allTangoAccounts, accountId)) {
      continue;
    }

    bool bToAdd = false;

    bToAdd |= (bPotentialTM && contactImpl->isPotentialAtmUser());
    bToAdd |= (bATM && contactImpl->getContactType() == sgiggle::corefacade::contacts::CONTACT_TYPE_ATM);
    bToAdd |= (bTango && contactImpl->getContactType() == sgiggle::corefacade::contacts::CONTACT_TYPE_TANGO);
    bToAdd |= (bOther && (contactImpl->getContactType() == sgiggle::corefacade::contacts::CONTACT_TYPE_NON_TANGO && !contactImpl->isPotentialAtmUser()));
    
    if (bToAdd) {
      allContacts->push_back(ContactPointer(new Contact(contactImpl)));
      allTangoAccounts.insert(accountId);
      SGLOG_TRACE("ContactManager::" << __FUNCTION__<< " add contact "<<contactImpl->getDisplayName()
                  << "("<<contactImpl->getHash() << ","<<contactImpl->getAccountId() << ")");
    }
  }

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": return count:" << allContacts->size());
  return allContacts;
}

ContactVectorPointer ContactManager::getFreePstnCallList() const
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);

  ContactVectorPointer allContacts(new ContactVector);

  if (!isBlockedListReady()) {
    return allContacts;
  }

  const std::string selfAccountId = getMyself().getAccountId();

  sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());

  BOOST_FOREACH (const ContactImplPointer& contactImpl, m_addressBookSortedList) {
    const std::string &accountId = contactImpl->getAccountId();

    if (!selfAccountId.empty() && accountId == selfAccountId) {
      continue;
    }

    if (contactImpl->isFreePstnCallQualified()) {
      allContacts->push_back(ContactPointer(new Contact(contactImpl)));
      SGLOG_TRACE("ContactManager::" << __FUNCTION__<< " add contact "<< contactImpl->getDisplayName()
                  << "("<<contactImpl->getHash() << ", "<<contactImpl->getAccountId() << ")");
    }
  }

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": return count:" << allContacts->size());
  return allContacts;
}

void ContactManager::clearCreatedTime(const std::vector<std::string>& outdated)
{
  pr::scoped_lock guard(m_mutex);
  
  foreach(it, outdated) {
    LET(it_ab, m_addressBookContacts.find(*it));
    if(it_ab != m_addressBookContacts.end()) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", reset " 
          << it_ab->second->getDisplayName() << "(" << it_ab->second->getHash() << ")");
      it_ab->second->setCreatedTime(0);
      it_ab->second->setContactChanged();
    }
  }
  // will finally populate to m_lastNotifyContacts
}

ContactVectorPointer ContactManager::getRecommendationList()
{
  sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", size= " << m_recommendationList->size());

  BOOST_FOREACH (ContactPointer contactImpl, *m_recommendationList) {
    if (!contactImpl->isRecommended()) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": m_recommendationList outdated!") ;
          
      POST_IMPl_THIS_IN_THREAD(DispatcherThread::get_timer_dispatcher(),
          calculateRecommendationList);

      break;
    }
  }

  return ContactVectorPointer(new ContactVector(m_recommendationList->begin(), m_recommendationList->end()));
}

void ContactManager::calculateHighlightedContactList()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);
  
  ContactVectorPointer highlightedList = ContactVectorPointer(new ContactVector);
  std::vector<std::string> outdated;
  
  {
    sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
    
    BOOST_FOREACH (ContactImplMultiMap::value_type& entry, m_tangoContactsForQuery) {
      BOOST_FOREACH(ContactImplPointer contactImpl, entry.second) {
        if (contactImpl->shouldBeHilighted(&outdated)) {
          SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", add: "
                      << contactImpl->getDisplayName() << ", hash=" << contactImpl->getHash());
          highlightedList->push_back(ContactPointer(new Contact(contactImpl)));
        } 
      }
    }
  }
  
  clearCreatedTime(outdated);

  {
    sgiggle::pr::scoped_wlock guard(m_query_rwmutex.wmutex());
    m_highlightedContactList.swap(highlightedList);
  }
}

void ContactManager::calculateRecommendationList() 
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);

  ContactVectorPointer recommendationList = ContactVectorPointer(new ContactVector);
  std::map<uint64_t, ContactImplVector> temp_map;
  std::vector<std::string> outdated;

  {
    sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());

    int temp_count = 0;
    foreach (it, m_addressBookSortedList) {
      if ((*it)->isRecommended(&outdated)) {
        temp_map[(*it)->getCreatedTime()].push_back((*it));
        ++temp_count;
      } 
    }
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", temp_count: " << temp_count);
  }

  clearCreatedTime(outdated);

  uint32_t old_recommendation_hash = m_recommendationHash,
           new_recommendation_hash = 0;
  size_t old_size = m_recommendationList->size(),
         new_size = 0;;

  int i = 0, count = 0;
  // we use ordered map to do the sorting
  reverse_foreach(rit, temp_map) {
    foreach(it, rit->second) {
      // TODO: breakout the nested loop earlier.
      if((uint64_t)((*it)->getCreatedTime()) > m_last_viewed_recommendation_timestamp) {
        ++count;
      }
      if(++i <= m_config_contact_recommendation_max) {
        SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", add: " << (*it)->getDisplayName() << ", " << (*it)->getSid());
        recommendationList->push_back(ContactPointer(new Contact(*it)));
        new_recommendation_hash ^= (*it)->getFullHash();
      }
    }
  }
  
  new_size = recommendationList->size();
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__  
      << ", oldSize = " << old_size
      << ", newSize = " << new_size
      << ", oldHash = " << old_recommendation_hash 
      << ", newHash = " << new_recommendation_hash);

  {
    sgiggle::pr::scoped_wlock guard(m_query_rwmutex.wmutex());
    m_recommendationList.swap(recommendationList);
    m_recommendationHash = new_recommendation_hash;
    // no need to lock, but I want to update in the same time so that the
    // ui won't get confused.
    m_new_recommendation_count = count;
  }

  if( new_recommendation_hash != old_recommendation_hash || new_size != old_size ) {
    notifyRecommendationCallbacks_();
  }
}

bool ContactManager::isInRecommendationList(const std::string& hash) const
{
  sgiggle::pr::scoped_wlock guard(m_query_rwmutex.wmutex());
  BOOST_FOREACH(ContactPointer c, *m_recommendationList) {
    if (c->getHash() == hash)
      return true;
  }
  return false;
}

bool ContactManager::isRecommended(uint64_t createdTime,
                                   const std::string& hash, 
                                   std::vector<std::string>* pOutdated) 
{
  if(/*quick exit*/!createdTime) {
    return false;
  }
  uint64_t now = pr::time_val::now().to_uint64();
  if(now - createdTime < (uint64_t)m_config_contact_recommendation_period) {
    return true;
  } else if (pOutdated) {
    pOutdated->push_back(hash);
  }
  return false;
}

// only return those have not been viewed for the badge
// NB: not limited by m_config_contact_recommendation_max
int ContactManager::getNewRecommendationCount() const
{
  return m_new_recommendation_count;
}

// immediately update the m_new_recommendation_count as 0 since no one is newer than 'now'.
// post the update of m_last_viewed_recommendation_timestamp which is used in next calculation.
// NB: notify callbacks when necessary
void ContactManager::setRecommendationListViewed()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ );
  
  if(m_new_recommendation_count) {
    m_new_recommendation_count = 0;
    notifyRecommendationCallbacks_();
  } // else m_new_recommendation_count = 0;
  
  setRecommendationListViewed_();
}

void ContactManager::setRecommendationListViewed_()
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD(DispatcherThread::get_timer_dispatcher(), setRecommendationListViewed_);
  m_last_viewed_recommendation_timestamp = pr::time_val::now().to_uint64();
  // NB: save to disk
  xmpp::UserSettings::getInstance()->set_setting<uint64_t>(CONTACT_LAST_VIEWED_RECOMMENDATION_TIMESTAMP, m_last_viewed_recommendation_timestamp);
}

ContactVectorPointer ContactManager::getHighlightedContactList()
{
  sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", size = " << m_highlightedContactList->size());

  BOOST_FOREACH (ContactPointer contactImpl, *m_highlightedContactList) {
    if (!contactImpl->shouldBeHilighted()) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": m_highlightedContactList outdated!") ;
          
      POST_IMPl_THIS_IN_THREAD(DispatcherThread::get_timer_dispatcher(),
          calculateHighlightedContactList);

      break;
    }
  }
  
  return ContactVectorPointer(new ContactVector(m_highlightedContactList->begin(), m_highlightedContactList->end()));
}

bool ContactManager::shouldHighlight(uint64_t createdTime,
                                     const std::string& hash, 
                                     std::vector<std::string>* pOutdated) 
{
  if(/*quick exit*/!createdTime) {
    return false;
  }
  uint64_t now = pr::time_val::now().to_uint64();
  if(now - createdTime < (uint64_t)m_config_contact_higlight_period) {
    return true;
  } else if (pOutdated) {
    pOutdated->push_back(hash);
  }
  return false;
}

void ContactManager::unhilightContact(const std::string& hash)
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD2(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), unhilightContact, hash);

  if (hash.empty()) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": empty hash");
    return;
  }
  {
    pr::scoped_lock lock(m_mutex);

    ContactImplMap::iterator it = m_addressBookContacts.find(hash);
    if (it == m_addressBookContacts.end()) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": hash " << hash << " not found");
      return;
    }

    if (it->second->isDeleted() || !it->second->shouldBeHilighted()) {
      return;
    }

    SGLOG_DEBUG("ContactManager::" << __FUNCTION__
                << ", hash=" << hash
                << ", displayName= " << it->second->getDisplayName());
    it->second->setCreatedTime(0);
    it->second->setContactChanged();
  }
  
  notifyServiceCallbacks_();
}

/**
 * called in webuser_request.cpp when contact_filter_response is used.
 * It will be called for each contact followed by broadcastContactFilterEvents() in the end.
 * It is called when a PTM user gets promoted to ATM
 *
 * We change m_lastNotifyContacts and m_tangoContactsForQuery maps at the same time because webuser handler
 * will search for those contacts immediately after addTangoContactsFromWebRequest is called.
 *
 * We change displayname slightly to make sure calculateDifference() will find difference so that notifyServiceCallbacks_ will be called.
 */
void ContactManager::addTangoContactsFromWebRequest(const std::vector<std::string>& hashVector,
                                                    const std::vector<std::string>& accountIdVector,
                                                    const std::vector<std::map<std::string, std::string> >& capabilitiesVector)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", size=" << hashVector.size());
  
  // add to m_addressBookContacts and m_tangoContacts
  {
    pr::scoped_lock lock(m_mutex);
    
    ContactImplMap social_friend_map;
    LET(hashIt, hashVector.begin());
    LET(accountIdIt, accountIdVector.begin());
    LET(capIt, capabilitiesVector.begin());
    for (; hashIt != hashVector.end(); hashIt++, accountIdIt++, capIt++) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": hash: " << *hashIt << ", accountId: " << *accountIdIt << ", caps=" << (*capIt).size());
      // we only do so, if it's not a tango friend probably due to contact-filter
      if (ContactImplPointer contactImpl = getAddressBookContactByHash_(*hashIt)) {
        if( !contactImpl->isDeleted() && contactImpl->getContactType() != sgiggle::corefacade::contacts::CONTACT_TYPE_TANGO) {
          addTangoContactFromContactFilter_(*hashIt, *accountIdIt, *capIt, social_friend_map);
        }
      }
    }// for
  }
  
  // add to m_lastNotifyContacts and m_tangoContactsForQuery
  {
    LET( hashIt, hashVector.begin());
    LET( accountIdIt, accountIdVector.begin());
    LET( capIt, capabilitiesVector.begin());
    for (; hashIt != hashVector.end(); hashIt++, accountIdIt++, capIt++) {
      sgiggle::pr::scoped_wlock guard(m_query_rwmutex.wmutex());
      
      ContactImplMap::iterator itAddrBook = m_lastNotifyContacts.find(*hashIt);
      
      if(itAddrBook == m_lastNotifyContacts.end()) {
        SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ": " << *hashIt << " no found. NOTE: ATM should be found.");
        continue;
      }
      
      if(itAddrBook->second->isDeleted()) {
        SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ": " << *hashIt << " no found. NOTE: deleted.");
        continue;
      }
      
      if(itAddrBook->second->getContactType() == sgiggle::corefacade::contacts::CONTACT_TYPE_TANGO) {
        SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ": " << *hashIt << " is already a social friend.");
        continue;
      }
      
      ContactImplPointer contactImpl = itAddrBook->second;
      contactImpl->setAccountId(*accountIdIt);
      contactImpl->setCapabilities(*capIt);
      // we change this to make sure calculateDifference() will
      // find difference so that notifyServiceCallbacks_ will be called.
      contactImpl->setDisplayName(contactImpl->getDisplayName() + " ");
      handleSocialMerge_(contactImpl, &m_lastNotifyContacts, &m_tangoContactsForQuery);
      addToTangoContacts_(contactImpl, &m_tangoContactsForQuery);
      // because the assumption is that the foreground/snapshot has
      // full-hash calculated
      contactImpl->calculateFullHash();
    }
  }
  
  notifyServiceCallbacks_(true);
}

bool ContactManager::addNewAddrBookContact_(ContactImplPointer contactImpl)
{
  contactImpl->setCreatedTime();
  ContactImplMap::iterator iter = m_addressBookContacts.find(contactImpl->getHash());
  if (iter != m_addressBookContacts.end()) {
    // XXX: this can definitely happen if store_thread load the address book first.
    // actually, I'm not sure why we want to have ContactManager::addNewAddrBookContact() API
    // since the address book change will soon be notified.
    // SG_ASSERT(false).
    contactImpl = iter->second;
  } else {
    m_addressBookContacts[contactImpl->getHash()] = contactImpl;
    m_addressBookContactsOverallHash ^= contactImpl->getHashUint32();
  }
  
  // so that to dedup the update requests if a filter request is already sent.
  if(!contactImpl->isFromAddressbook()) {
    contactImpl->setFromAddressbook(true);
    contactImpl->setContactChanged();
    return true;
  }
  
  return false;
}

void ContactManager::addContactUpdateBatch_(ContactImplVector& contactList, std::vector<std::string>& hashes)
{
  if (!contactList.empty()) {
    sgiggle::xmpp::ProcessorImpl::pointer processor = sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessorImpl();
    
    NewContactUpdateBatch batch;
    
    int seq = ++m_nextContactUpdateSeq;
    batch.hashes = hashes;
    batch.new_contact_request = tango::httpme::new_contact_request::create(tango::httpme::contact_filter_request::get_url(),
                                                                           processor,
                                                                           v3::filteraccount::OPT_OUT,
                                                                           seq,
                                                                           contactList);
    batch.new_contact_request->send();
    batch.timer_id = processor->SetTimer(CONTACT_UPDATE_TIMEOUT, boost::bind(&ContactManager::contactUpdateDone,
                                                                             this,
                                                                             seq,
                                                                             /*bSucceed*/false,
                                                                             /*hashes*/std::vector<std::string>(),
                                                                             /*accountIds*/std::vector<std::string>(),
                                                                             /*capabilities*/std::vector<std::map<std::string, std::string> >()));
    m_contactUpdateBatchMap[seq] = batch;
  }
}

// XXX: actually, I'm not sure why we want to have ContactManager::addNewAddrBookContact() API
// since the address book change will soon be notified.
void ContactManager::addNewAddrBookContact1(ContactImplPointer contactImpl)
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD2(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(),
                                     addNewAddrBookContact1, contactImpl);

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", hash=" << contactImpl->getHash()
              << ", displayName=" << contactImpl->getDisplayName() << ", deviceId=" << contactImpl->getDeviceContactId());

  pr::scoped_lock lock(m_mutex);

  std::stringstream compoundId;
  compoundId << contactImpl->getDeviceContactId();

  ContactImplVector contactList;
  std::vector<std::string> hashes;

  for (size_t i = 0; i < 2; i++) {
    for (size_t j = 0; j < (i == 0 ? contactImpl->getPhoneNumberSize() : contactImpl->getEmailSize()); j++) {
      std::vector<sgiggle::contacts::PhoneNumber> phoneNumbers;
      std::vector<std::string> emails;
      if (i == 0 /* phone-number */) {
        corefacade::contacts::PhoneNumberPointer pnp = contactImpl->getPhoneNumberAt(j);
        SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", add phone number:" << pnp->getFormattedString());
        phoneNumbers.push_back(*pnp);
      } else /* email */{
        SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", add email:" << contactImpl->getEmailAt(j));
        emails.push_back(contactImpl->getEmailAt(j));
      }
      
      ContactImplPointer contactImpl2 = ContactImplPointer(new ContactImpl(contactImpl->getNamePrefix(),
                                                                           contactImpl->getFirstName(),
                                                                           contactImpl->getMiddleName(),
                                                                           contactImpl->getLastName(),
                                                                           contactImpl->getNameSuffix(),
                                                                           contactImpl->getDisplayName(),
                                                                           emails,
                                                                           phoneNumbers,
                                                                           contactImpl->getDeviceContactId(),
                                                                            /*has_picture*/false));
      contactImpl2->setCompoundId(compoundId.str());
      
      if(addNewAddrBookContact_(contactImpl2)) {
        SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", "<< contactImpl->getDisplayName() << "(" << contactImpl->getSid() << ")");
        contactList.push_back(contactImpl2);
        hashes.push_back(contactImpl2->getHash());
      }
    }
  }
  addContactUpdateBatch_(contactList, hashes);
}

// In-app add a contact, and pass into the added record id \p deviceContactId.
// XXX: actually, I'm not sure why we want to have ContactManager::addNewAddrBookContact() API
// since the address book change will soon be notified.
void ContactManager::addNewAddrBookContact2(int64_t deviceContactId) {
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD2(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(),
                                     addNewAddrBookContact2, deviceContactId);

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", deviceContactId=" << deviceContactId);

  pr::scoped_lock lock(m_mutex);

  boost::unordered_map<std::string, ContactImplPointer> contactsMap;
  ContactImplVector contactList;
  std::vector<std::string> hashes;

  if (!store_.loadContactsByDeviceContactId((int)deviceContactId, contactsMap)) {
    SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ", load failed.");
    return;
  }

  BOOST_FOREACH(ContactImplMap::value_type& entry, contactsMap) {
    ContactImplPointer contactImpl = entry.second;
    SG_ASSERT(contactImpl->getPhoneNumberSize() <= 1);
    SG_ASSERT(contactImpl->getEmailSize() <= 1);
    
    if(addNewAddrBookContact_(contactImpl)) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__  << ", " << contactImpl->getDisplayName() << "(" << contactImpl->getSid() << ")");
      contactList.push_back(contactImpl);
      hashes.push_back(contactImpl->getHash());
    }
  }
  addContactUpdateBatch_(contactList, hashes);
}

// TODO: what's this for?
bool ContactManager::addNonFriendContactIfNotExist(const Contact& c)
{
  SG_ASSERT(!c.getAccountId().empty());
  
  if (getTangoContact_(c.getAccountId(), false, true)) {
    return false;
  }
  
  return social::ProfileSimpleFetcher::getInstance()->addContact(c, /*server_time_in_ms*/0, /*bAddIfNotExists*/false);
}

//
void ContactManager::handleContactFilterDone(const std::string& session_id, contact_filter_request::ContactFilteringResultEnum res)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ <<" Address book synced in server "
              << res << ", filterSize=" << m_filterResultMap.size());
  
  bool bForceNotify = false;
  
  {
    pr::scoped_lock lock(m_mutex);
    
    if (session_id != m_contactFilterSessionId) {
      SGLOG_DEBUG("ContactManager::"<<__FUNCTION__<<", bad session id, skip");
      return;
    }
    setContactResolveFinished_();
    
    ContactImplMap social_friend_map;
    foreach (it, m_addressBookContacts) {
      ContactImplPointer contactImpl = it->second;
      if (!contactImpl->isFromAddressbook() || contactImpl->isDeleted()) {
        continue;
      }
      
      FilterResultMap::iterator itFilter = m_filterResultMap.find(it->first);
      if (itFilter == m_filterResultMap.end()) {
        // for a from-address-book contact, it's updated in: filter and update.
        // make sure we won't overwrite the update-result.
        if(!contactImpl->getContactUpdateResultField()) {
          bForceNotify |= asNativeAddrBookAndNewSocial_(contactImpl, social_friend_map);
        }
      } else {
        bForceNotify |= addTangoContactFromContactFilter_(itFilter->first, itFilter->second.accountId, itFilter->second.capabilities, social_friend_map);
      }
    }
    
    newSocialContacts_(social_friend_map);
    
    if (contact_filter_request::ADDRESS_BOOK_SYNCED_WITH_SERVER == res) {
      m_addressBookHashBasedOnWhichContactsAreFilteredLastTime = m_addressBookHashBasedOnWhichContactsAreFilteredForCurrentCycle;
      m_addressBookHashBasedOnWhichContactsAreFilteredForCurrentCycle = 0;
      sgiggle::xmpp::UserInfo::getInstance()->setAddressBookHashBasedOnWhichContactsAreFilteredLastTime(m_addressBookHashBasedOnWhichContactsAreFilteredLastTime);
      SGLOG_DEBUG("ContactManager::"<<__FUNCTION__<<" overall hash saved "<< m_addressBookHashBasedOnWhichContactsAreFilteredLastTime);
      
      m_countrycodeBasedOnWhichContactsAreFilteredLastTime = m_countrycodeBasedOnWhichContactsAreFilteredForCurrentCycle ;
      m_countrycodeBasedOnWhichContactsAreFilteredForCurrentCycle = "";
      sgiggle::xmpp::UserInfo::getInstance()->setUserCountryCodeBasedOnWhichContactsAreFilteredLastTime(m_countrycodeBasedOnWhichContactsAreFilteredLastTime);
      
      m_lastContactFilterTime = sgiggle::pr::time_val::now().to_uint64();
      sgiggle::xmpp::UserInfo::getInstance()->setLastContactFilterTime(m_lastContactFilterTime);
    } else if (contact_filter_request::ADDRESS_BOOK_SYNC_OPT_OUT == res) {
      m_addressBookHashBasedOnWhichContactsAreFilteredLastTime = 0;
      m_addressBookHashBasedOnWhichContactsAreFilteredForCurrentCycle = 0;
      m_countrycodeBasedOnWhichContactsAreFilteredForCurrentCycle = "";
      sgiggle::xmpp::UserInfo::getInstance()->setAddressBookHashBasedOnWhichContactsAreFilteredLastTime(m_addressBookHashBasedOnWhichContactsAreFilteredLastTime);
      SGLOG_DEBUG("ContactManager::"<<__FUNCTION__<<" overall hash saved "<<m_addressBookHashBasedOnWhichContactsAreFilteredLastTime);
      
      m_lastContactFilterTime = sgiggle::pr::time_val::now().to_uint64();
      sgiggle::xmpp::UserInfo::getInstance()->setLastContactFilterTime(m_lastContactFilterTime);
    }
 
    m_1stTimeContactFilterSucceeded = true;
    
    sgiggle::xmpp::UserInfo::getInstance()->save();
 
    //TO migrate from CRC32 based hash to SID Hash. Do cleanupAfterMigration priro to sa
    cleanupAfterMigration__();
    sgiggle::xmpp::UserInfo::getInstance()->setPersistentContactFilterVersion(sgiggle::xmpp::UserInfo::PERSISTENT_CONTACT_UNENCODED_SID_HASH_VERSION);
  } //lock scope

  bForceNotify |= m_bForceNotify;
  m_bForceNotify = false;
  notifyServiceCallbacks_(bForceNotify);
}

void ContactManager::contactUpdateDone(int seq_id,
                                       bool bSucceed,
                                       const std::vector<std::string>& hashes,
                                       const std::vector<std::string>& accountIds,
                                       const std::vector<std::map<std::string, std::string> >& capabilities)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", seq=" << seq_id << ", bSucceed=" << bSucceed);
  {
    pr::scoped_lock lock(m_mutex);

    ContactUpdateBatchMap::iterator it = m_contactUpdateBatchMap.find(seq_id);
    if (it == m_contactUpdateBatchMap.end()) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", seq " << seq_id << " not found");
      return;
    }

    sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessorImpl()->CancelTimer(it->second.timer_id);
    it->second.new_contact_request->cancel();
    it->second.new_contact_request.reset();

    SG_ASSERT(hashes.size() == accountIds.size() && hashes.size() == capabilities.size());
    
    // set flag
    foreach(it_hash, it->second.hashes) {
      ContactImplMap::iterator iterAB = m_addressBookContacts.find(*it_hash);
      if (iterAB == m_addressBookContacts.end() || iterAB->second->isDeleted()) {
        SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ", not found or deleted");
        continue;
      }
      SG_ASSERT(!iterAB->second->getContactUpdateResultField());
      if(bSucceed) {
        iterAB->second->setContactUpdateResultField(UPDATE_CONTACT_RESULT_RETURNED);
      } else {
        iterAB->second->setContactUpdateResultField(UPDATE_CONTACT_RESULT_RETURNED | UPDATE_CONTACT_RESULT_TIMED_OUT);
      }
      iterAB->second->setContactChanged();
    }

    // update data
    ContactImplMap social_friend_map;
    LET(it_hash, hashes.begin());
    LET(it_acct, accountIds.begin());
    LET(it_cap, capabilities.begin());
    for(; it_hash != hashes.end(); ++it_hash, ++it_acct, ++it_cap) {
      addTangoContactFromContactFilter_(*it_hash, *it_acct, *it_cap, social_friend_map);
    }
    newSocialContacts_(social_friend_map);
    
    // cleanup the batch
    m_contactUpdateBatchMap.erase(it);
    
    if (!bSucceed) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", force contact filter");
      m_addressBookHashBasedOnWhichContactsAreFilteredLastTime = m_addressBookContactsOverallHash - 1; // make sure to be different to force contact filter
      setNeedToContactUpdate();
    }
  }
  
  notifyServiceCallbacks_(true);
}

void ContactManager::cancelAllContactUpdates()
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), cancelAllContactUpdates);

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);

  pr::scoped_lock lock(m_mutex);

  bool bContactUpdateNeedFilter = false;
  for (ContactUpdateBatchMap::iterator iter = m_contactUpdateBatchMap.begin(); iter != m_contactUpdateBatchMap.end(); iter++)  {
    sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessorImpl()->CancelTimer(iter->second.timer_id);
    iter->second.new_contact_request->cancel();
    iter->second.new_contact_request.reset();
    bContactUpdateNeedFilter = true;
  }
  m_contactUpdateBatchMap.clear();

  if (bContactUpdateNeedFilter) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", force contact filter");
    m_addressBookHashBasedOnWhichContactsAreFilteredLastTime = m_addressBookContactsOverallHash - 1; // make sure to be different to force contact filter
    setNeedToContactUpdate();
  }
}

void ContactManager::setState_(ContactManagerState state)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": state=" << getStateName(state) << ", old_state=" << getStateName(m_state));
  m_state = state;
}

std::string ContactManager::getStateName(ContactManagerState state) const
{
  std::string name = "NoName";
  switch(state) {
  case CONTACTMGR_STATE_ADDRESS_BOOK_NOT_ALLOWED:
    name = "AddressBookNotAllowed";
    break;
  case CONTACTMGR_STATE_ADDRESS_BOOK_NOT_LOADED:
    name = "AddressBookNotLoaded";
    break;
  case CONTACTMGR_STATE_LOADING_ADDRESS_BOOK:
    name = "AddressBookLoading";
    break;
  case CONTACTMGR_STATE_ADDRESS_BOOK_LOADED:
    name = "AddressBookLoaded";
    break;
  case CONTACTMGR_STATE_CONTACT_FILTERING:
    name = "ContactFiltering";
    break;
  default:
    SG_ASSERT(false);
  }
  return name ;
}

// we keep blocked and hidden contacts in the m_addressBookSortedList because we want to save them in the local storage
// because we want them to be searchable by account id.
ContactImplVector ContactManager::sortByCollation(const ContactImplMap& contacts) const
{
  bool bSMSAvailable = isSMSAvailable();
  bool bEmailAvailable = isEmailAvailable();
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", size=" << contacts.size() << ", sort_order=" << m_sort_order << ", sms=" << bSMSAvailable << ", email=" << bEmailAvailable);

  driver::GenericDriver * devDriver = driver::getFromRegistry(driver::type::LocalizationUtility);
  SG_ASSERT(devDriver!=NULL);
  sgiggle::driver::LocalizationUtility* localizationUtilityDriver = reinterpret_cast<sgiggle::driver::LocalizationUtility*>(devDriver);

  std::vector<sgiggle::corefacade::util::KeyValuePair> kv_pairs;
  std::vector<sgiggle::corefacade::util::KeyValuePair> kv_pairs_bottom; // for contacts displayed at bottom
  ContactImplMap::const_iterator iter;
  for (iter = contacts.begin(); iter != contacts.end(); ++iter)
  {
    if (m_isStopping)
      return ContactImplVector();

    ContactImplPointer contactImpl = iter->second;
    if (contactImpl->isDeleted())
      continue;

    if (contactImpl->getContactType() != sgiggle::corefacade::contacts::CONTACT_TYPE_TANGO)
    {
      if (contactImpl->getPhoneNumberSize() > 0)
      {
        SG_ASSERT(contactImpl->getPhoneNumberSize() == 1);
        if (!bSMSAvailable)
          continue;
        if (contactImpl->getPhoneNumbers()[0].getSubscriberNumber().length() < Contact::MIN_PHONE_NUMBER_LENGTH)
        {
          SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", skip invalid phone number " << contactImpl->getPhoneNumbers()[0].getSubscriberNumber());
          continue;
        }
      }
      if (contactImpl->getEmailSize() > 0)
      {
        SG_ASSERT(contactImpl->getEmailSize() == 1);
        if (!bEmailAvailable)
          continue;
        if (!sgiggle::common::mail_validator::isValid(contactImpl->getEmailAt(0).c_str()))
        {
          SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", skip invalid email " << contactImpl->getEmailAt(0));
          continue;
        }
      }
    }
    std::string full_name;
    std::string first_name = contactImpl->getFirstName();
    std::string last_name = contactImpl->getLastName();
    if (first_name.empty())
    {
      if (last_name.empty())
        full_name = contactImpl->getDisplayName();
      else
        full_name = last_name;
    }
    else if (last_name.empty())
      full_name = first_name;
    else if (m_sort_order != sgiggle::corefacade::contacts::CONTACT_ORDER_LAST_NAME)
      full_name = first_name + " " + last_name;
    else
      full_name = last_name + " " + first_name;

    std::string sectionTitle;
#if defined(ANDROID)
    char c = full_name.at(0);
    if (isalpha(c))
    {
      sectionTitle = (islower(c) ? toupper(c) : c);
    }
    else
    {
      sectionTitle = "#";
      full_name = "#" + full_name;
    }
#else
    sectionTitle = localizationUtilityDriver->getCollationKey(full_name);
#endif

    std::transform(full_name.begin(), full_name.end(), full_name.begin(), ::tolower);

    if (contactImpl->getAccountId().empty()) // make sure contacts with accountId appears first
      full_name += "|1|";
    else
      full_name += "|0|";
    if (contactImpl->getPhoneNumberSize() > 0)
    {
      PhoneNumber pn = contactImpl->getPhoneNumbers()[0];
      std::stringstream ss;
      if (pn.phoneType == sgiggle::corefacade::contacts::PHONE_TYPE_MOBILE || pn.phoneType == sgiggle::corefacade::contacts::PHONE_TYPE_IPHONE) // invitable social ids
   	    full_name += "0";
      else if (pn.phoneType == sgiggle::corefacade::contacts::PHONE_TYPE_WORK) // count as ptm
        full_name += "1";
      else
        full_name += "2";
      full_name += pn.countryCode + "-" + pn.subscriberNumber;
    }
    else
      full_name += contactImpl->getDefaultEmail();

    contactImpl->setSectionTitle(sectionTitle);
    sgiggle::corefacade::util::KeyValuePair kv;

    kv.key = full_name;
    kv.value = iter->first;
    if (sectionTitle == "#") {
      // If the contact belongs to section #, we insert it into a separate vector. We will sort two vectors
      // separately in order to always display section # contacts on the bottom. DRGN-4861
      // The code may need to be changed accordingly if more types of sections other than A-Z & # are defined
      // by OS or Tango code in the future.
      kv_pairs_bottom.push_back(kv);
    }
    else {
      kv_pairs.push_back(kv);
    }
  }

  // To make sure that section # contacts will always be displayed at bottom, we sort two kvp sets separately and
  // append the second (bottom) sorted set to the first one. DRGN-4861
  localizationUtilityDriver->sortByCollation(kv_pairs);
  localizationUtilityDriver->sortByCollation(kv_pairs_bottom);
  kv_pairs.insert(kv_pairs.end(), kv_pairs_bottom.begin(), kv_pairs_bottom.end());

  ContactImplVector sorted_list;
  ContactImplPointer lastContactImpl;
  std::map<std::string, ContactImplPointer> accountIds; // with the same Display name
  std::map<std::string, ContactImplPointer> compoundIds; // for no account Id ones
  BOOST_FOREACH (const sgiggle::corefacade::util::KeyValuePair& kvp, kv_pairs)
  {
    if (m_isStopping)
      return ContactImplVector();

    std::string hash = kvp.value;

    ContactImplMap::const_iterator iter = contacts.find(hash);
    if (iter == contacts.end())
    {
      continue;
    }

    ContactImplPointer contactImpl = iter->second;

    if (lastContactImpl)
    {
  	  bool bSameName = equals_no_case(lastContactImpl->getDisplayName(), contactImpl->getDisplayName());
  	  if (!bSameName)
      {
        accountIds.clear();
        compoundIds.clear();
      }

  	  std::map<std::string, ContactImplPointer>::iterator accountIdIter, compoundIdIter;

      if (!contactImpl->getAccountId().empty())
      {
        accountIdIter = accountIds.find(contactImpl->getAccountId());

        compoundIdIter = compoundIds.find(contactImpl->getCompoundId());
        if (compoundIdIter == compoundIds.end())
          compoundIds[contactImpl->getCompoundId()] = (accountIdIter != accountIds.end() ? accountIdIter->second : contactImpl);
        else
        {
          SG_ASSERT(!compoundIdIter->second->getAccountId().empty());
          // no need to set bFreePstnCallQualified because they will split if account id are different
          if (compoundIdIter->second->getAccountId() > contactImpl->getAccountId())
            compoundIdIter->second = contactImpl;
        }

        if (accountIdIter != accountIds.end())
        {
          SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", skip tango contact " << contactImpl->getDisplayName() << ", accountId=" << contactImpl->getAccountId()
            << ", compoundId=" << contactImpl->getCompoundId() << ", sid=" << contactImpl->getSid());
          contactImpl->setNotToBeDisplayed(true);
          sorted_list.push_back(contactImpl); // to save it into local storage
          if (contactImpl->isInternalFreePstnCallQualified())
            accountIdIter->second->setFreePstnCallQualified(true);
          continue;
        }
        accountIds[contactImpl->getAccountId()] = contactImpl;
        contactImpl->setFreePstnCallQualified(contactImpl->isInternalFreePstnCallQualified());
      }
      else
      {
        compoundIdIter = compoundIds.find(contactImpl->getCompoundId());
        if (compoundIdIter != compoundIds.end())
        {
          contactImpl->setNotToBeDisplayed(true);
          if (contactImpl->isInternalFreePstnCallQualified())
            compoundIdIter->second->setFreePstnCallQualified(true);
          SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", skip contact " << contactImpl->getDisplayName() << ", accountId=" << contactImpl->getAccountId()
            << ", compoundId=" << contactImpl->getCompoundId() << ", sid=" << contactImpl->getSid());
          continue;
        }
        compoundIds[contactImpl->getCompoundId()] = contactImpl;
        contactImpl->setFreePstnCallQualified(contactImpl->isInternalFreePstnCallQualified());
      }
    }
    else
    {
      if (!contactImpl->getAccountId().empty())
        accountIds[contactImpl->getAccountId()] = contactImpl;
      compoundIds[contactImpl->getCompoundId()] = contactImpl;
      contactImpl->setFreePstnCallQualified(contactImpl->isInternalFreePstnCallQualified());
    }

    contactImpl->setNotToBeDisplayed(false);
    sorted_list.push_back(contactImpl);
    lastContactImpl = contactImpl;

    std::string contactType;
    switch (contactImpl->getContactType())
    {
    case sgiggle::corefacade::contacts::CONTACT_TYPE_TANGO:
      contactType = "tango";
      break;
    case sgiggle::corefacade::contacts::CONTACT_TYPE_ATM:
      contactType = "atm";
      break;
    default:
      contactType = contactImpl->isPotentialAtmUser() ? "ptm" : "other";
    }
    SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", after sort, " << contactImpl->getDisplayName() << ", title=" << contactImpl->getSectionTitle() << ", type=" << contactType << ", accountId=" << contactImpl->getAccountId()
      << ", compoundId=" << contactImpl->getCompoundId() << ", sid=" << contactImpl->getSid() << ", isTF=" << contactImpl->isFromSocial() << ", isAB=" << contactImpl->isFromAddressbook());
  }

  return sorted_list;
}

bool ContactManager::useCRC32Hash__() const
{
  return (xmpp::UserInfo::getInstance()->getPersistentContactFilterVersion() < xmpp::UserInfo::PERSISTENT_CONTACT_UNENCODED_SID_HASH_VERSION );
}

/**
 * called in media_engine/LoginState.cpp
 *
 * Handle the event in which this client first enters the UILoginState or UILoginCompletedState.
 *
 */
void ContactManager::handleIfItIsTheFirstTimeEnteringContactListTab()
{
}

ContactImplPointer ContactManager::getAddressBookContactByHash_(const std::string& hash) const
{
  return getAddressBookContactByHash_(hash, Contact::getCurrentHashVersion());
}

ContactImplPointer ContactManager::getAddressBookContactByHash_(const std::string& hash, int hashVersion) const
{
  ContactImplMap::const_iterator itAddrBook;

  if (hashVersion == Contact::getCurrentHashVersion()) {
    // quick search using current hash
    itAddrBook = m_addressBookContacts.find(hash);
    if (itAddrBook != m_addressBookContacts.end()) {
      return itAddrBook->second;
    }
  } else {
    // long search using old hash
    for (itAddrBook = m_addressBookContacts.begin(); itAddrBook != m_addressBookContacts.end(); itAddrBook++) {
      if (itAddrBook->second->getHash(hashVersion) == hash)
        return itAddrBook->second;
    }
  }

  return ContactImplPointer();
}

int ContactManager::getDeviceContactIdByAccountId(const std::string& accountId) const
{
  ContactImplPointer c = getTangoContact_(accountId, false, true);
  if (c) {
    return c->getDeviceContactId();
  }
  
  return contacts::DEVICE_CONTACT_ID_UNKNOWN;
}

std::string ContactManager::getDisplaynameByAccountId(const std::string& accountId, const std::string& defaultDisplayName) const
{
  std::string displayName;
  
  ContactImplPointer c = getTangoContact_(accountId, true, true);
  if (c && !c->getDisplayName().empty()) {
    return c->getDisplayName();
  }
  
  return defaultDisplayName;
}

bool ContactManager::getTangoContact(Contact& c, const std::string& accountId) const
{
  if (ContactImplPointer contactImpl = getTangoContact_(accountId, true, true)) {
    c.setImpl(contactImpl);
    return true;
  }
  return false;
}

ContactPointer ContactManager::getTangoContact(const std::string &accountId) const
{
  ContactPointer cp(new Contact());
  if (getTangoContact(*cp, accountId)) {
    return cp;
  } else {
    return ContactPointer();
  }
}

ContactImplPointer ContactManager::getTangoContact_(const std::string& accountId,
                                                    bool bCheckSocialTangoContact,
                                                    bool checkBlockedAndHiddenContact,
                                                    bool bLightMode) const
{
  std::string phoneNumber = "unknown";
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " look for " << accountId << ", checkSocial=" << bCheckSocialTangoContact << ", checkBlockedAndHidden=" << checkBlockedAndHiddenContact);
  
  if (accountId.empty()) {
    return ContactImplPointer();
  }
  
  {
    sgiggle::pr::scoped_lock guard(m_quick_mutex);
    
    if (accountId == m_myself->getAccountId()) {
      return m_myself;
    }
  }
  
  {
    sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
    
    LET(foundIt, m_tangoContactsForQuery.find(accountId));
    if (foundIt != m_tangoContactsForQuery.end()) {
      SG_ASSERT(!foundIt->second.empty());
      
      ContactImplPointer contactImpl = foundIt->second[0];
      SG_ASSERT(contactImpl->getAccountId() == accountId);
      
      const std::vector<PhoneNumber> phoneNumbers = contactImpl->getPhoneNumbers();
      if (!phoneNumbers.empty()) {
        phoneNumber = phoneNumbers.front().getSubscriberNumber();
      }
      if (contactImpl->isBlocked() || contactImpl->isHidden()) {
        if (checkBlockedAndHiddenContact)  {
          SGLOGF_DEBUG("ContactManager::%s find in blocked or hidden list, phone: %s", __FUNCTION__, phoneNumber.c_str());
          contactImpl->setAccountId(accountId);
          return contactImpl;
        } else {
          return ContactImplPointer();
        }
      }
      // XXX: what?
      if (!contactImpl->isFromAddressbook() && !bCheckSocialTangoContact) {
        return ContactImplPointer();
      }
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " found in m_tangoContacts, phone=" << phoneNumber << ", deviceContactId" << contactImpl->getDeviceContactId());
      
      return contactImpl;
    } else if (bCheckSocialTangoContact) {
      return sgiggle::social::ProfileSimpleFetcher::getInstance()->getSocialContact(accountId, (bLightMode ? sgiggle::social::PSF_NO_FETCH : sgiggle::social::PSF_SYNC_FETCH));
    }
  }
  
  return ContactImplPointer();
}

ContactPointer ContactManager::getContactByHash(const std::string &hash) const
{
  SGLOG_TRACE("ContactManager::getContactByHash, hash=" << hash);
  
  {
    sgiggle::pr::scoped_lock guard(m_quick_mutex);
    
    if (hash == m_myself->getHash())
      return ContactPointer(new Contact(m_myself));
  }
  {
    sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
    
    ContactImplMap::const_iterator iter = m_lastNotifyContacts.find(hash);
    if (iter != m_lastNotifyContacts.end())
    {
      SGLOG_DEBUG("ContactManager::getContactByHash, return accountId=" << iter->second->getAccountId());
      return ContactPointer(new Contact(iter->second));
    }
  }
  
  if (isSocialOnly(hash)) {
    if (ContactImplPointer contactImpl = getTangoContact_(getAccountIdForSocialOnly(hash), true, true, true))
      return ContactPointer(new Contact(contactImpl));
  }
  
  return ContactPointer();
}

bool ContactManager::getContactByHash(xmpp::Contact& c, const std::string &hash) const
{
  ContactPointer contact = getContactByHash(hash);
  if (!contact)
    return false;
  fillProtobufContact(&c, *contact);
  return true;
}

bool ContactManager::hasSubscriberNumber_(const ContactImplPointer contactImpl, const std::string &subscriberNumber) const
{
  BOOST_FOREACH(const PhoneNumber& pn, contactImpl->getPhoneNumbers()) {
    std::string phone = pn.getSubscriberNumber();
    if (phone.length() >= subscriberNumber.length()) {
      if(phone.substr(phone.length() - subscriberNumber.length()) == subscriberNumber ) {
        return true;
      }
    } else if(subscriberNumber.substr(subscriberNumber.length() - phone.length()) == phone) {
      return true;
    }
  }
  return false;
}

bool ContactManager::getContactBySubscriberNumber(Contact &c, const std::string &subscriberNumber) const
{
  sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
  
  std::string key = (subscriberNumber.length() < NUMBER_OF_DIGITS_AS_KEY_FOR_PHONE_NUMBER ?
                     subscriberNumber :
                     subscriberNumber.substr(subscriberNumber.length() - NUMBER_OF_DIGITS_AS_KEY_FOR_PHONE_NUMBER));
  
  PhoneNumberContactMultiMap::const_iterator it = m_phonenumbertoContact.find(key);
  if (it == m_phonenumbertoContact.end()) {
    return false;
  }
  
  BOOST_FOREACH(const ContactImplPointer& contactImpl, it->second) {
    if(hasSubscriberNumber_(contactImpl, subscriberNumber)) {
      c.setImpl(contactImpl);
      return true;
    }
  }
  return false;
}

std::vector<ContactImplPointer> ContactManager::getContactsBySubscriberNumber(const std::string &subscriberNumber) const
{
  sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
  
  std::vector<ContactImplPointer> ret_v;
  
  std::string key = subscriberNumber.length() < NUMBER_OF_DIGITS_AS_KEY_FOR_PHONE_NUMBER ? subscriberNumber : subscriberNumber.substr(subscriberNumber.length() - NUMBER_OF_DIGITS_AS_KEY_FOR_PHONE_NUMBER);
  PhoneNumberContactMultiMap::const_iterator it = m_phonenumbertoContact.find(key);
  if (it == m_phonenumbertoContact.end())
    return ret_v;
  
  BOOST_FOREACH(const ContactImplPointer& contactImpl, it->second) {
    if (hasSubscriberNumber_(contactImpl, subscriberNumber)) {
      ret_v.push_back(contactImpl);
    }
  }
  return ret_v;
}

/*
 * for validation_request, registration_request
 */
void ContactManager::resetContacts()
{
  pr::scoped_lock lock(m_mutex);

  SGLOGF_TRACE("Resetting ab contacts.");
  m_addressBookContacts.clear();
  m_tangoContacts.clear();

  store_.save(contact_store_proxy::contacts());
  // This API is mainly called for the PC code path
}

// for starting AddressBookSyncTask
void ContactManager::resetAddressbookSync_()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);
  
  ab_sync_temp_.clear();
  cancelContactResolve();
  
  // will be cancelled in commitAddressBookSync
  rescheduleContactResolveTimer();
}


// for each round in AddressBookSyncHandler, AddressBookSyncTask.
void ContactManager::rescheduleAddressBookSyncTimer()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);
  
  pr::scoped_lock guard(m_mutex);
  
  cancelAddressBookSyncTimer_();
  // XXX: the timeout function!!!
  m_addressBookSyncTimer = sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor()->SetTimer(
                                                                                                      ADDRESS_BOOK_SYNC_TIMEOUT, boost::bind(&ContactManager::contactResolveTimeout_, this, "XXX"));
}

// XmppSessionImpl
// XXX: I think XmppSessionImpl is to stop the task instead of the timer.
void ContactManager::cancelAddressBookSyncTimer_()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);
  if (m_addressBookSyncTimer != -1) {
    sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor()->CancelTimer(
                                                                                  m_addressBookSyncTimer);
    m_addressBookSyncTimer = -1;
  }
}

// AddressBookSyncHandler.cpp
void ContactManager::addToAddressbookSync(const std::list<Contact>& contacts)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);
  
  pr::scoped_lock lock(m_mutex);
  for (std::list<Contact>::const_iterator it = contacts.begin(); it != contacts.end(); it++) {
    ContactImplPointer ptr(new ContactImpl(*it));
    ab_sync_temp_.push_back(ptr);
  }
}

// AddressBookSyncHandler.cpp
void ContactManager::commitAddressBookSync()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);

  uint32_t overallHash = 0;
  {
    pr::scoped_lock lock(m_mutex);

    cancelContactResolveTimer_();
    cancelAddressBookSyncTimer_();
    
    m_addressBookContacts.clear();
    m_tangoContacts.clear();

    for (std::list<ContactImplPointer>::const_iterator it = ab_sync_temp_.begin(); it != ab_sync_temp_.end(); ++it) {
      std::string h = (*it)->getHash();
      m_addressBookContacts[h] = *it;
      if (!(*it)->getAccountId().empty()) {
        addToTangoContacts_(*it);
      }
      overallHash ^= (*it)->getHashUint32();
    }
    m_addressBookContactsOverallHash = (uint32_t)overallHash;
    store_.save(ab_sync_temp_);
    ab_sync_temp_.clear();
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", Address-Book committed size = " << m_addressBookContacts.size());
  }

  notifyServiceCallbacks_(true);
}

void ContactManager::setNeedToRecalculatePstnCallValue()
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), setNeedToRecalculatePstnCallValue);
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);

  bool bNotify = false;
  {
    pr::scoped_lock lock(m_mutex);
    m_bPstnRuleSet = true;
    bNotify = m_bPstnRuleSet != m_bPstnRuleSetNotify;
  }
  if (calculatePstnCallValue() || bNotify) {
    notifyServiceCallbacks_();
  }
}

// return true if any pstn flag changes
bool ContactManager::calculatePstnCallValue()
{
  SG_ASSERT(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor()->is_in_service_thread());

  pr::scoped_lock lock(m_mutex);
  
  bool bChanged = false;
  foreach (iter, m_addressBookContacts) {
    if (m_isStopping) {
      break;
    }

    ContactImplPointer contactImpl = iter->second;
    if (contactImpl->getPhoneNumberSize() > 0) {
      corefacade::contacts::PhoneNumberPointer pnp = contactImpl->getDefaultPhoneNumber();
      bool bFreePstnCallQualified = countrycodes::FreePstnCallCheck::checkPhoneNumber(pnp->subscriberNumber);
      if (contactImpl->isInternalFreePstnCallQualified() != bFreePstnCallQualified) {
        contactImpl->setInternalFreePstnCallQualified(bFreePstnCallQualified);
        contactImpl->setContactChanged();
        bChanged = true;
      }
    }
  }

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", bChanged=" << bChanged);
  return bChanged;
}

bool ContactManager::contactsAreLoading() const
{
  driver::GenericDriver* drv = driver::getFromRegistry(
      driver::type::ContactStore);
  return drv != NULL && ((ContactStore *) (drv))->contactsAreLoading();
}

bool ContactManager::isContactsLoadedFirstTime() const
{
  return m_1stTimeAddressbookAndCachedLoaded;
}

bool ContactManager::isPstnListReady() const
{
  return m_bPstnRuleSetNotify && this->isAddressBookLoaded();
}

/*
 * called in Stop()
 */
void ContactManager::setContactLoadingEnabled(bool enabled)
{
  driver::GenericDriver* drv = driver::getFromRegistry(driver::type::ContactStore);

  if (drv != NULL) {
    ((ContactStore *) (drv))->setContactLoadingEnabled(enabled);
  } else  {
    SGLOGF_WARN("Contact loader driver is NULL. Could not %s contact loading.", (enabled ? "enable" : "disable"));
  }
}

/*
 * called in cient_app/iphone/ui/TangoAppDelegate.mm
 * XXX: implementation?
 */
void ContactManager::resetAddressBookTimer()
{
}

/*
 * called in registration_request.cpp
 */
bool ContactManager::adaptAddressBookContactsAll(ContactAdapterInterface& adapter)
{
  sgiggle::pr::scoped_lock lock(m_mutex);
  const std::string selfAccountId = getMyself().getAccountId();

  ContactImplMap addressBookContacts;
  ContactImplMap::const_iterator citer;
  for (citer = m_addressBookContacts.begin(); citer != m_addressBookContacts.end(); citer++) {
    if (!citer->second->isFromAddressbook() || citer->second->isDeleted())
      continue;

    const std::string &accountId = citer->second->getAccountId();
    if (accountId.empty() || accountId != selfAccountId) {
      addressBookContacts.insert(*citer);
    }
  }

  citer = addressBookContacts.begin();
  int packedContacts = adapter.adaptRange(addressBookContacts, citer, (std::numeric_limits<size_t>::max)());
  if (packedContacts > 0)
    return true;
  else
    return false;
}

/*
 * called by media_engine/InviteeContactListAdapter.cpp
 */
void ContactManager::adaptNonTangoContacts(ContactAdapterInterface& adapter) const
{
  sgiggle::pr::scoped_lock lock(m_mutex);
  ContactImplMap nonTangoContacts;
  ContactImplMap::const_iterator iter;
  for (iter = m_addressBookContacts.begin(); iter != m_addressBookContacts.end(); ++iter) {
    if (iter->second->getAccountId() == "" && !iter->second->isDeleted()) {
      nonTangoContacts.insert(std::make_pair(iter->first, iter->second));
    }
  }
  adapter.adapt(nonTangoContacts);
}

/*
 * called in corefacade/tango/corefacade/atm/impl/AtmServiceImpl.cpp
 */
void ContactManager::adaptAllContacts(ContactAdapterInterface& adapter) const
{
  pr::scoped_lock lock(m_mutex);
  const std::string selfAccountId = getMyself().getAccountId();

  ContactImplMap allContacts;
  std::set<std::string> allTangoAccounts;
  BOOST_FOREACH(const ContactImplMap::value_type& entry, m_addressBookContacts) {
    if (entry.second->isDeleted())
      continue;

    const std::string &accountId = entry.second->getAccountId();
    if (selfAccountId.empty() || accountId != selfAccountId) {
      allContacts.insert(entry);
    }

    if (!accountId.empty()) {
      allTangoAccounts.insert(accountId);
    }
  }

  adapter.adapt(allContacts);
}

void ContactManager::getLocalContactAccountIdList(std::list<std::string> &contactAccountList) const
{
  sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
  for (ContactImplMultiMap::const_iterator it = m_tangoContactsForQuery.begin(); it != m_tangoContactsForQuery.end(); it++) {
    SG_ASSERT(!it->second.empty());
    if (!it->second[0]->isBlocked() && !it->second[0]->isHidden() && it->second[0]->isFromAddressbook()) {
      contactAccountList.push_back(it->first);
    }
  }
}

// return true for contacts in local address book, no matter he/she is blocked or not.
bool ContactManager::isInLocalAddressbook(const std::string &accountId) const
{
  sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
  LET(foundIt, m_tangoContactsForQuery.find(accountId));
  if (foundIt == m_tangoContactsForQuery.end()) {
    return false;
  }
  SG_ASSERT(!foundIt->second.empty());
  return foundIt->second[0]->isFromAddressbook();
}

Contact ContactManager::getMyself() const
{
  xmpp::UserInfo* userinfo = xmpp::UserInfo::getInstance();
  sgiggle::pr::scoped_lock guard(m_quick_mutex);
  if (m_myself->calculateCRC32Hash() != userinfo->getContactHash()) {
    m_myself->clear();
    m_myself->setNamePrefix(userinfo->nameprefix());
    m_myself->setFirstName(userinfo->firstname());
    m_myself->setMiddleName(userinfo->middlename());
    m_myself->setLastName(userinfo->lastname());
    m_myself->setNameSuffix(userinfo->namesuffix());
    m_myself->addPhoneNumber(PhoneNumber(userinfo->countrycodenumber(), sgiggle::phone_formatter::removeFormat(userinfo->subscribernumber()),
      sgiggle::corefacade::contacts::PHONE_TYPE_GENERIC));
    m_myself->addEmailAddress(userinfo->email());
    m_myself->setDisplayName(userinfo->getDisplayName());
    m_myself->resetCRC32Hash(); // reset to calculate later
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", copy from UserInfo, my_hash=" << m_myself->calculateCRC32Hash()
        << ", his=" << userinfo->getContactHash() << ", displayName=" << m_myself->getDisplayName());
  }
  if (m_myself->getAccountId() != userinfo->accountid()) {
    m_myself->setAccountId(userinfo->accountid());
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", reset accountId to " << m_myself->getAccountId());
  }

  return Contact(m_myself);
}

bool ContactManager::isMyselfHash(const std::string& hash) const
{
  Contact c = getMyself();
  return c.getHash() == hash;
}

bool ContactManager::isMyselfAccountId(const std::string& accountId) const
{
  Contact c = getMyself();
  return !c.getAccountId().empty() && c.getAccountId() == accountId;
}

bool ContactManager::lookupTangoContactByAccountId(const std::string& accountId,
                                                   Contact&           contact,
                                                   bool               checkNonFriendTangoContact,
                                                   bool               checkBlockedAndHiddenLocalContact,
                                                   bool               bLightMode) const
{
  if (ContactImplPointer foundContact = getTangoContact_(accountId, checkNonFriendTangoContact, checkBlockedAndHiddenLocalContact, bLightMode)) {
    contact.setImpl(foundContact);
    return true;
  }
  SGLOGF_DEBUG("ContactManager::%s %s failed", __FUNCTION__, accountId.c_str());
  return false;
}

// returns the contact if there's only one contact with name and associated with the given accountId
// otherwise return the social contact if it has a name, otherwise return an empty pointer
// if bSearchSocial is false, use defaultSocialContact instead of getting profile from ProfileServiceImpl
ContactImplPointer ContactManager::getPreferredContactByAccountId(const std::string& accountId, bool bSearchSocial, ContactImplPointer defaultSocialContact) const
{
  SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", accountId=" << accountId << ", bSearchSocial=" << bSearchSocial << ", default=" << (defaultSocialContact ? defaultSocialContact->getDisplayName() : "_null"));
  if (accountId.empty())
    return ContactImplPointer();

  ContactImplPointer contactImplPreferred, contactImplCandidate, contactFoundInContactManager;
  int64_t deviceContactId = DEVICE_CONTACT_ID_UNKNOWN;
  bool bAmbiguityFound = false;
  {
    sgiggle::pr::scoped_lock guard(m_quick_mutex);

    if (accountId == m_myself->getAccountId())
      return m_myself;
  }
  {
    sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());

    ContactImplMultiMap::const_iterator foundIt = m_tangoContactsForQuery.find(accountId);
    if (foundIt != m_tangoContactsForQuery.end() && foundIt->second.size() > 0) {
      contactFoundInContactManager = *foundIt->second.begin();
      BOOST_FOREACH(ContactImplPointer contactImpl, foundIt->second) {
        if (deviceContactId == DEVICE_CONTACT_ID_UNKNOWN && contactImpl->hasNativePicture()) {
          deviceContactId = contactImpl->getDeviceContactId();
        }
        if (contactImpl->getFirstName().empty() && contactImpl->getLastName().empty()) {
          if (!contactImplCandidate && !contactImpl->getPureDisplayName().empty()) {
            contactImplCandidate = contactImpl;
          }
          continue;
        }
        if (!contactImplPreferred || (contactImpl->hasNativePicture() && !contactImplPreferred->hasNativePicture())) {
          contactImplPreferred = contactImpl;
          deviceContactId = contactImpl->getDeviceContactId();
        } else if (contactImplPreferred->getDisplayName() != contactImpl->getDisplayName()) {
          bAmbiguityFound = true;
        }
      } // foreach
    } // find(accountId)
  }

  SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", preferred=" << (contactImplPreferred ? contactImplPreferred->getDisplayName() : "_null") << ", bAmbiguityFound=" << bAmbiguityFound);
  if (!contactImplPreferred || bAmbiguityFound) {
    ContactImplPointer contactImpl;
    if (bSearchSocial) {
      contactImpl = social::ProfileSimpleFetcher::getInstance()->getSocialContact(accountId, (defaultSocialContact ? sgiggle::social::PSF_ASYNC_FETCH : sgiggle::social::PSF_SYNC_FETCH));
    }
    if (!contactImpl) {
      contactImpl = defaultSocialContact;
    }

    if (contactImpl) {
      if (contactFoundInContactManager) {
        ContactImplPointer contactImplCombined = ContactImplPointer(new ContactImpl(*contactFoundInContactManager));
        SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", cm=" << contactFoundInContactManager->getAccountId() << ", combined=" << contactFoundInContactManager->getAccountId() << ", ci=" << contactImpl->getAccountId());
        SG_ASSERT(contactImplCombined->getAccountId() == contactImpl->getAccountId());
        contactImplCombined->setFirstName(contactImpl->getFirstName());
        contactImplCombined->setLastName(contactImpl->getLastName());
        contactImplCombined->setThumbnailUrl(contactImpl->getThumbnailUrlDirect());
        contactImplCombined->setThumbnailPath(contactImpl->getThumbnailPathDirect());
        contactImpl = contactImplCombined;
      }

      if (!contactImpl->getFirstName().empty() || !contactImpl->getLastName().empty()) {
        contactImplPreferred = contactImpl;
      } else if (!contactImplCandidate) {
        contactImplCandidate = contactImpl;
      }
    }
  }

  if (!contactImplPreferred && contactImplCandidate) {
    contactImplPreferred = contactImplCandidate;
  }

  if (!contactImplPreferred || !contactImplPreferred->getThumbnailUrl().empty()
      || contactImplPreferred->getDeviceContactId() == deviceContactId || deviceContactId == DEVICE_CONTACT_ID_UNKNOWN) {
    return contactImplPreferred;
  }

  SG_ASSERT(!contactImplPreferred->hasNativePicture());
  contactImplPreferred = ContactImplPointer(new ContactImpl(*contactImplPreferred));
  contactImplPreferred->setHasNativePicture(true);
  contactImplPreferred->setDeviceContactId(deviceContactId);
  return contactImplPreferred;
}

int ContactManager::tangoCount() const
{
  sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
  int count = 0;
  ContactImplMultiMap::const_iterator it;
  for (it = m_tangoContactsForQuery.begin(); it != m_tangoContactsForQuery.end(); it++) {
    if (!it->second.empty() && it->second[0]->isBlocked() && it->second[0]->isHidden()) {
      continue;
    }
    count += it->second.size();
  }

  return count;
}

int ContactManager::getContactCount(sgiggle::corefacade::contacts::ContactType type) const
{
  sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());

  int count = 0;
  BOOST_FOREACH(const ContactImplMap::value_type &entry, m_lastNotifyContacts) {
    if (!entry.second->isDeleted() && !entry.second->isBlocked() && !entry.second->isHidden() && entry.second->getContactType() == type) {
      count++;
    }
  }

  return count;
}

// XXX:
int ContactManager::addressBookCount() const
{
  sgiggle::pr::scoped_lock guard(m_mutex);
  return m_addressBookContacts.size();
}

void ContactManager::broadcastContactFilterEvents(bool only_if_accountid_updated)
{
  SGLOG_DEBUG("In ContactManager::" << __FUNCTION__ << ", OBSOLETED!!!");
}

void ContactManager::resetContactFilterSessionId_()
{
  std::stringstream ss;
  ss << sgiggle::pr::monotonic_time::now().to_msec();
  m_contactFilterSessionId = ss.str();
  
  // associated variables
  m_seq_number = 0;
  m_current_it = m_addressBookContacts.begin();
}

ContactStore::Tango_Contact_Resolve_Type ContactManager::getTangoContactResolveType()
{
  return store_.get_type();
}

void ContactManager::loadCachedContacts_()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": ENTER.");
  
  xmpp::PersistentContactList cachedContacts;
  if (!xmpp::UserInfo::getInstance()->loadTangoContact(cachedContacts)) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": load failed");
    sgiggle::xmpp::UserInfo::getInstance()->setPersistentContactFilterVersion(sgiggle::xmpp::UserInfo::PERSISTENT_CONTACT_UNENCODED_SID_HASH_VERSION);
    return;
  }
  
  ContactImplMap addressBookContacts;
  ContactImplMultiMap tangoContacts;
  ContactImplMap lastNotifyContacts;
  ContactImplMultiMap tangoContactsForQuery;
  ContactImplVector addressBookSortedList;
  PhoneNumberContactMultiMap phonenumbertoContact;
  
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " contacts cached: " << cachedContacts.contacts_size());
  for (int i = 0; i < cachedContacts.contacts_size(); ++i) {
    const xmpp::PersistentContact& contact = cachedContacts.contacts(i);
    
    //Copied into list
    ContactImplPointer contactImpl = PersistentContact2ContactImpl(contact);
    ContactImplPointer contactImplNotify = boost::make_shared<ContactImpl>(contactImpl);
    if (!useCRC32Hash__() ) {
      addressBookContacts[contactImpl->getHash()] = contactImpl;
      lastNotifyContacts[contactImpl->getHash()] = contactImplNotify;
      SGLOG_TRACE(__FUNCTION__<<" loaded contact from cache with "<< contactImpl->getHash() << " display_name "<<contactImpl->getDisplayName());
    } else {
      contactImpl->setUnderHashMigration__() ;
      addressBookContacts[contactImpl->getCRC32Hash()] = contactImpl;
      lastNotifyContacts[contactImpl->getCRC32Hash()] = contactImplNotify;
      SGLOG_TRACE(__FUNCTION__<<" loaded contact from cache with CRC32 "<< contactImpl->getCRC32Hash()<< " display_name "<<contactImpl->getDisplayName());
    }
    addressBookSortedList.push_back(contactImplNotify);
    SG_ASSERT(!contactImpl->getAccountId().empty());
    tangoContacts[contactImpl->getAccountId()].push_back(contactImpl);
    tangoContactsForQuery[contactImpl->getAccountId()].push_back(contactImplNotify);
    indexContactByPhoneNumber(contactImplNotify, &phonenumbertoContact);
    
    SGLOG_TRACE("ContactManager::" << __FUNCTION__ << ", display_name=" << contactImpl->getDisplayName() << ", accountId=" << contactImpl->getAccountId());
    contactImpl->setContactChanged();
  }
  
  {
    pr::scoped_lock lock(m_mutex);
    m_addressBookContacts.swap(addressBookContacts);
    m_tangoContacts.swap(tangoContacts);
    
    m_1stTimeAddressbookAndCachedLoaded |= m_addressBookContacts.size() > 0;
  }
  
  {
    sgiggle::pr::scoped_wlock guard(m_query_rwmutex.wmutex());
    m_lastNotifyContacts.swap(lastNotifyContacts);
    m_tangoContactsForQuery.swap(tangoContactsForQuery);
    m_addressBookSortedList.swap(addressBookSortedList);
    m_phonenumbertoContact.swap(phonenumbertoContact);
  }
  
  calculateHighlightedContactList();
  // at this time, we don't have recommendation to calculate since
  // only the Tango-social friends are saved in the disk.
  
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " end");
}

void ContactManager::saveTangoContactsToLocalStorage_()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);

  xmpp::PersistentContactList persistentList;
  persistentList.set_hashversion(Contact::getCurrentHashVersion());

  {
    sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
    BOOST_FOREACH(ContactImplPointer contactImpl, m_addressBookSortedList)
    {
      if (contactImpl->getAccountId().empty())
        continue;

      xmpp::PersistentContact *ct = persistentList.add_contacts();
      syncPersistentContactWithContact(ct, contactImpl);
      
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__
                  << ": " << contactImpl->getDisplayName() << "(" << contactImpl->getHash() << ")");
    }
  }

  xmpp::UserInfo::getInstance()->setUserContactCache(persistentList);
}

void ContactManager::loadCachedTime_()
{
  if (m_bInviteTimeLoaded) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": already done!");
    return;
  }
  
  xmpp::PersistentContactList cachedInvites;
  if (!xmpp::UserInfo::getInstance()->getUserInviteCache(cachedInvites)) {
    SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ": failed!");
    m_bInviteTimeLoaded = true;
    return;
  }
  
  for (int i = 0; i < cachedInvites.contacts_size(); ++i) {
    const xmpp::PersistentContact& contact = cachedInvites.contacts(i);
    
    // check contacts by hash
    if (ContactImplPointer c = getAddressBookContactByHash_(contact.hash(), cachedInvites.hashversion())) {
      c->setInviteTime(contact.invitetime());
      c->setCreatedTime(contact.createdtime());
      c->setContactChanged();
      
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ":(" << contact.hash() << ")");
    }
  }
  
  m_bInviteTimeLoaded = true;
}

/*
 * since load time is not done on start, thus check read before write.
 */
void ContactManager::saveTimeToLocalStorage_()
{
  if(!m_bInviteTimeLoaded) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": haven't read. Don't save.");
    return;
  }
  
  xmpp::PersistentContactList persistentList;
  persistentList.set_hashversion(Contact::getCurrentHashVersion());

  {
    sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
    for (ContactImplMap::const_iterator it = m_lastNotifyContacts.begin(); it != m_lastNotifyContacts.end(); ++it)
    {
      const contacts::ContactImplPointer& contactImpl = it->second;
      if (!contactImpl->isDeleted() &&
          contactImpl->getContactType() != sgiggle::corefacade::contacts::CONTACT_TYPE_TANGO &&
          ( contactImpl->getInviteTime() > 0 || contactImpl->getCreatedTime() > 0 ) ) {
        xmpp::PersistentContact *ct = persistentList.add_contacts();
        ct->set_hash(contactImpl->getHash());
        ct->set_invitetime(contactImpl->getInviteTime());
        ct->set_createdtime(contactImpl->getCreatedTime());
        
        SGLOG_DEBUG("ContactManager::" << __FUNCTION__
                    << ": " << contactImpl->getDisplayName() << "(" << contactImpl->getHash() << ")");
      }
    }
  }
  
  xmpp::UserInfo::getInstance()->setUserInviteCache(persistentList);
}

/*static */void ContactManager::syncPersistentContactWithContact(xmpp::PersistentContact *ct, const contacts::ContactImplPointer& contact)
{
  ct->set_nameprefix(contact->getNamePrefix());
  ct->set_firstname(contact->getFirstName());
  ct->set_middlename(contact->getMiddleName());
  ct->set_lastname(contact->getLastName());
  ct->set_namesuffix(contact->getNameSuffix());
  ct->set_displayname(contact->getDisplayName());
  ct->set_accountid(contact->getAccountId());
  // This call - generates the CRC
  ct->set_hash(contact->getHash());
  ct->set_invitetime(contact->getInviteTime());

  const std::vector<PhoneNumber> phoneNumbers = contact->getPhoneNumbers();
  if (!phoneNumbers.empty())
  {
    const contacts::PhoneNumber phoneNumber = phoneNumbers.front();
    sgiggle::xmpp::CountryCode* countrycode = ct->mutable_phonenumber()->mutable_countrycode();
    countrycode->set_countryid(""); // fake Country-Id.
    countrycode->set_countryname(""); // fake Country-Name.
    countrycode->set_countrycodenumber(phoneNumber.getCountryCode());

    *(ct->mutable_phonenumber()->mutable_subscribernumber()) = phoneNumber.getSubscriberNumber();
    ct->mutable_phonenumber()->set_type(session::nativeToProtobufPhoneType(phoneNumber.getType()));
  }

  const std::vector<std::string> emailAddresses = contact->getEmailAddresses();
  if (! emailAddresses.empty())
  {
    ct->set_email(emailAddresses.front());
  }

  typedef std::map<std::string, std::string> Capabilities;
  const Capabilities capabilities = contact->getCapabilities();
  BOOST_FOREACH (const Capabilities::value_type &capability, capabilities)
  {
    xmpp::KeyValuePair *keyValuePair = ct->add_capability();
    keyValuePair->set_key(capability.first);
    keyValuePair->set_value(capability.second);
  }
  ct->set_createdtime(contact->getCreatedTime());
  ct->set_isfromaddressbook(contact->isFromAddressbook());
  ct->set_isfromsocial(contact->isFromSocial());
  ct->set_isnottobedisplayed(contact->isNotToBeDisplayed());
}

std::vector<std::string> ContactManager::getAllAccountIds() const
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);

  sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());

  std::vector<std::string> ret_l;

  BOOST_FOREACH(const ContactImplMultiMap::value_type& entry, m_tangoContactsForQuery) {
    SG_ASSERT(!entry.second.empty());
    if (!entry.second[0]->isBlocked() && !entry.second[0]->isHidden())
      ret_l.push_back(entry.first);
  }

  return ret_l;
}

/*static */std::string ContactManager::getAccountIdForSocialOnly(const std::string& hash)
{
  return hash.substr(strlen(CONTACT_HASH_PREFIX_FOR_TANGO_FRIEND));
}

/*static */bool ContactManager::isSocialOnly(const std::string& hash)
{
  return starts_with(hash, CONTACT_HASH_PREFIX_FOR_TANGO_FRIEND);
}

/*static */std::string ContactManager::composeHashByAccountIdForSocialOnly(const std::string& accountId)
{
  return CONTACT_HASH_PREFIX_FOR_TANGO_FRIEND + accountId;
}

/*static */bool ContactManager::isTangoMember(const std::string& name)
{
  return name == sgiggle::localization::LocalizedStringContainer::getInstance()->get(sgiggle::corefacade::localization::LS_TANGO_MEMBER);
}

/*
 * called in getContactByHash()
 */
/*static */void ContactManager::fillProtobufContact(xmpp::Contact * ct, Contact& contact)
{
  if (!ct)
  {
    SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ": xmpp::contact is null.");
    return;
  }
  ct->set_nameprefix(contact.getNamePrefix());
  ct->set_firstname(contact.getFirstName());
  ct->set_middlename(contact.getMiddleName());
  ct->set_lastname(contact.getLastName());
  ct->set_namesuffix(contact.getNameSuffix());
  ct->set_displayname(contact.getDisplayName());
  ct->set_accountid(contact.getAccountId());
  ct->set_devicecontactid(contact.getDeviceContactId());
  ct->set_haspicture(contact.hasNativePicture());
  ct->set_hash(contact.getHash());
  ct->set_hash_int(contact.getHashUint32());
  ct->set_nativefavorite(contact.isNativeFavorite());
  ct->set_invitetime(contact.getInviteTime());

  const std::vector<contacts::PhoneNumber> phoneNumbers = contact.getPhoneNumbers();
  if (!phoneNumbers.empty())
  {
    // trying to use the mobile number since we even don't know which one is tango user
    bool found = false;
    for (std::vector<contacts::PhoneNumber>::const_iterator it = phoneNumbers.begin(); it != phoneNumbers.end(); ++it)
    {
      if (xmpp::PHONE_TYPE_MOBILE == session::nativeToProtobufPhoneType(it->getType())
      || xmpp::PHONE_TYPE_IPHONE == session::nativeToProtobufPhoneType(it->getType()))
      {
        found = true;
        ct->mutable_phonenumber()->set_subscribernumber(it->getSubscriberNumber());
        ct->mutable_phonenumber()->set_type(session::nativeToProtobufPhoneType(it->getType()));
      }
    }
    if (!found)
    {
      // no mobile number found, just use the first one.
      // format it
      bool matched = false;
      ct->mutable_phonenumber()->set_subscribernumber(phone_formatter::format(phoneNumbers.front().getSubscriberNumber(),
              phoneNumbers.front().getCountryCode(), &matched));
      if (matched)
      {
        ct->mutable_phonenumber()->mutable_countrycode()->set_countrycodenumber(phoneNumbers.front().getCountryCode());
      }
      ct->mutable_phonenumber()->set_type(session::nativeToProtobufPhoneType(phoneNumbers.front().getType()));
    }
  }

  // We always send email addresses to the UI if they are available.
  const std::vector<std::string> emailAddresses = contact.getEmailAddresses();
  if (!emailAddresses.empty())
  {
    ct->set_email(emailAddresses.front());
  }
}

/* static */bool ContactManager::isSMSAvailable()
{
  return tango::util::getDevInfoDriver()->get_minor_type() == sgiggle::init::DevInfo::DT_SMARTPHONE;
}

/* static */bool ContactManager::isEmailAvailable()
{
  return tango::util::getDevInfoDriver()->is_email_available();
}

/*
 * called in xmpp/XmppSessionImpl3.cpp
 */
/*static */void ContactManager::fillProtobufContact(xmpp::Contact * ct, const xmpp::PersistentContact& contact)
{
  if (!ct)
  {
    SGLOG_ERROR("ContactManager::" << __FUNCTION__ << ": xmpp::contact is null.");
    return;
  }
  ct->set_nameprefix(contact.nameprefix());
  ct->set_firstname(contact.firstname());
  ct->set_middlename(contact.middlename());
  ct->set_lastname(contact.lastname());
  ct->set_namesuffix(contact.namesuffix());
  ct->set_displayname(contact.displayname());
  ct->set_accountid(contact.accountid());
  if (contact.has_phonenumber())
    *(ct->mutable_phonenumber()) = contact.phonenumber();
  if (contact.has_email())
    ct->set_email(contact.email());
  ct->set_devicecontactid(DEVICE_CONTACT_ID_UNKNOWN);
  ct->set_nativefavorite(contact.nativefavorite());
  ct->set_invitetime(contact.invitetime());
  ct->set_hash(contact.hash());
}

/*static */ContactImplPointer ContactManager::PersistentContact2ContactImpl(const xmpp::PersistentContact& contact)
{
  ContactImplPointer contactImpl(new ContactImpl);

  contactImpl->setNamePrefix(contact.nameprefix());
  contactImpl->setFirstName(contact.firstname());
  contactImpl->setMiddleName(contact.middlename());
  contactImpl->setLastName(contact.lastname());
  contactImpl->setNameSuffix(contact.namesuffix());
  contactImpl->setDisplayName(contact.displayname());
  contactImpl->setAccountId(contact.accountid());
  if (contact.has_phonenumber())
  {
    contactImpl->addPhoneNumber(PhoneNumber(contact.phonenumber().countrycode().countrycodenumber(),
            contact.phonenumber().subscribernumber(), sgiggle::session::protobufToNativePhoneType(contact.phonenumber().type()) ));
  }
  if (contact.has_email())
  {
    contactImpl->addEmailAddress(contact.email());
  }
  contactImpl->setDeviceContactId(DEVICE_CONTACT_ID_UNKNOWN);
  contactImpl->setNativeFavorite(contact.nativefavorite());

  typedef std::map<std::string, std::string> Capabilities;
  Capabilities capabilities;
  BOOST_FOREACH (const xmpp::KeyValuePair &keyValuePair, contact.capability())
  {
    capabilities[keyValuePair.key()] = keyValuePair.value();
  }

  contactImpl->setCapabilities(capabilities);
  contactImpl->setCreatedTime(contact.createdtime());

  if (contact.has_hash() )
  {
    contactImpl->setHash(contact.hash());
  }

  contactImpl->setFromAddressbook(true);
  if (contact.has_isfromaddressbook())
    contactImpl->setFromAddressbook(contact.isfromaddressbook());

  contactImpl->setFromSocial(false);
  if (contact.has_isfromsocial())
    contactImpl->setFromSocial(contact.isfromsocial());

  contactImpl->setNotToBeDisplayed(false);
  if (contact.has_isnottobedisplayed())
    contactImpl->setNotToBeDisplayed(contact.isnottobedisplayed());

  return contactImpl;
}

/*static */void ContactManager::PersistentContact2Contact(Contact& /* OUT */c, const xmpp::PersistentContact& contact)
{
  c.setImpl(PersistentContact2ContactImpl(contact));
}

/*
 * called in sgiggle::tc::TCStorageManager::update_contact_hash()
 */
/*static */void ContactManager::protobufContact2Contact(Contact& c, const xmpp::Contact& contact)
{
  ContactImplPointer cPtr(new ContactImpl);

  cPtr->setNamePrefix(contact.nameprefix());
  cPtr->setFirstName(contact.firstname());
  cPtr->setMiddleName(contact.middlename());
  cPtr->setLastName(contact.lastname());
  cPtr->setNameSuffix(contact.namesuffix());
  cPtr->setDisplayName(contact.displayname());
  cPtr->setAccountId(contact.accountid());
  if (contact.has_phonenumber())
  {
	  cPtr->addPhoneNumber(PhoneNumber(contact.phonenumber().countrycode().countrycodenumber(),
            contact.phonenumber().subscribernumber(), sgiggle::session::protobufToNativePhoneType(contact.phonenumber().type()) ));
  }
  if (contact.has_email())
  {
	  cPtr->addEmailAddress(contact.email());
  }
  cPtr->setDeviceContactId(DEVICE_CONTACT_ID_UNKNOWN);
  cPtr->setNativeFavorite(contact.nativefavorite());

  c.setImpl(cPtr);
}

/*
 * called in contacts/ContactProviderBase.cpp
 */
void ContactManager::registerContactProvider(IContactProvider* provider)
{
}

/*
 * called in contacts/ContactProviderBase.cpp
 */
void ContactManager::unregisterContactProvider(IContactProvider* provider)
{
}

void ContactManager::registerUpdateContactResponseCallback(UpdateContactResponseCallback callback)
{
  pr::scoped_lock lock(m_quick_mutex);
  m_updateContactResponseCallback = callback;
}

void ContactManager::removeUpdateContactResponseCallback()
{
  pr::scoped_lock lock(m_quick_mutex);
  m_updateContactResponseCallback = NULL;
}

/*
 * called in corefacade/contacts/impl/ContactServiceImpl.cpp, content/impl/GameServiceImpl.cpp
 */
int ContactManager::registerServiceCallback(ContactUpdateCallback callback)
{
  int id;
  {
    sgiggle::pr::scoped_lock lock(m_quick_mutex);
    id = m_nextCallbackId ++;
    m_serviceUpdateCallbacks.insert(std::pair<int, ContactUpdateCallback>(id, callback));
  }
  sgiggle::corefacade::contacts::ContactVectorPointer added_contacts(new sgiggle::corefacade::contacts::ContactVector);
  sgiggle::corefacade::contacts::ContactVectorPointer changed_contacts(new sgiggle::corefacade::contacts::ContactVector);
  sgiggle::corefacade::contacts::ContactVectorPointer removed_contacts(new sgiggle::corefacade::contacts::ContactVector);
  {
    sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());

    BOOST_FOREACH(const ContactImplMap::value_type& entry, m_lastNotifyContacts) {
      if (entry.second->isDeleted() || entry.second->isNotToBeDisplayed())
        continue;
      ContactPointer c(new Contact(entry.second));
      added_contacts->push_back(c);
    }
  }
  // call immediately after register ret
  sgiggle::xmpp::ProcessorImpl::pointer processor = sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessorImpl();
  processor->SetTimer(10, boost::bind(callback, added_contacts, changed_contacts, removed_contacts, false));

  return id;
}

/*
 * called in corefacade/contacts/impl/ContactServiceImpl.cpp
 */
void ContactManager::removeServiceCallback(int callbackId)
{
  sgiggle::pr::scoped_lock lock(m_quick_mutex);
  m_serviceUpdateCallbacks.erase(callbackId);
}

int ContactManager::registerRecommendationCallback(ContactRecommendationCallback callback)
{
  sgiggle::pr::scoped_lock lock(m_quick_mutex);
  int id = m_nextCallbackId ++;
  m_recommendationCallbacks.insert(std::pair<int, ContactRecommendationCallback>(id, callback));
  return id;
}

/*
 * called in corefacade/contacts/impl/ContactServiceImpl.cpp
 */
void ContactManager::removeRecommendationCallback(int callbackId)
{
  sgiggle::pr::scoped_lock lock(m_quick_mutex);
  m_recommendationCallbacks.erase(callbackId);
}

/*
 * called in stop()
 */
void ContactManager::clearAllCallbacks()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", sz=" << m_serviceUpdateCallbacks.size());
  // we do this because when we clear the vector, those who register using shared_ptr might get destruct and call unregister during destruction
  std::map<int, ContactUpdateCallback> callbacks;
  {
    sgiggle::pr::scoped_lock lock(m_quick_mutex);
    callbacks.swap(m_serviceUpdateCallbacks);
  }
  callbacks.clear();
}

void ContactManager::assertTangoContacts_()
{
  # if !defined(NDEBUG)
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": m_tangoContacts");
  foreach(it, m_tangoContacts) {
    foreach(it2, it->second) {
      ContactImplPointer contactImpl = *it2;
      SGLOG_DEBUG( "    " << contactImpl->getAccountId() << ", " << contactImpl->getDisplayName() << "(" << contactImpl->getHash() << ")");
    }
  }
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": m_tangoContactsForQuery");
  foreach(it, m_tangoContactsForQuery) {
    foreach(it2, it->second) {
      ContactImplPointer contactImpl = *it2;
      SGLOG_DEBUG( "    " << contactImpl->getAccountId() << ", " << contactImpl->getDisplayName() << "(" << contactImpl->getHash() << ")");
    }
  }
  
  SG_ASSERT(m_tangoContacts.size() == m_tangoContactsForQuery.size());
  foreach(it_bg, m_tangoContacts) {
    LET(it_fg, m_tangoContactsForQuery.find(it_bg->first));
    SG_ASSERT(it_bg->second.size() == it_fg->second.size());
    /* XXX: vector cannot be compared in this way!!!
    LET(it_bg_v, it_bg->second.begin());
    LET(it_fg_v, it_fg->second.begin());
    for(; it_bg_v != it_bg->second.end(); ++it_bg_v, ++it_fg_v) {
      SG_ASSERT((*it_bg_v)->getHash() == (*it_fg_v)->getHash());
    } */
  }
  #endif
}

// calculate difference between m_addressBookContacts and m_lastNotifyContacts, along with m_tangoContactsForQuery, m_phonenumbertoContact
// it will also merge all contacts with same accountId or same display name if accountId is empty
void ContactManager::calculateContactsDifference_(sgiggle::corefacade::contacts::ContactVectorPointer added_contacts,
                                                  sgiggle::corefacade::contacts::ContactVectorPointer changed_contacts,
                                                  sgiggle::corefacade::contacts::ContactVectorPointer removed_contacts,
                                                  sgiggle::corefacade::contacts::ContactVectorPointer contactUpdatedSucceedContacts,
                                                  sgiggle::corefacade::contacts::ContactVectorPointer contactUpdatedTimedOutContacts)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);

  ContactImplMap lastNotifyContacts, new_lastNotifyContacts;
  ContactImplMultiMap new_tangoContactsForQuery;
  PhoneNumberContactMultiMap new_phonenumbertoContact;
  
  {
    sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
    lastNotifyContacts = m_lastNotifyContacts;
  }
    
  // add/changed
  BOOST_FOREACH(const ContactImplMap::value_type& entry, m_addressBookContacts) {
    ContactImplPointer contactImplBackground = entry.second;
    
    if (contactImplBackground->isContactChanged()) {
      contactImplBackground->calculateFullHash();
      contactImplBackground->resetContactChanged();
    }
    
    ContactImplPointer contactImplNotify = boost::make_shared<ContactImpl>(contactImplBackground.get());
    bool bAddToContactUpdatedList = false;
    LET(it, lastNotifyContacts.find(entry.first));
    if(it == lastNotifyContacts.end())  {
      bAddToContactUpdatedList = ((contactImplBackground->getContactUpdateResultField() & UPDATE_CONTACT_RESULT_NOTIFIED) == 0 &&
                                  (contactImplBackground->getContactUpdateResultField() & UPDATE_CONTACT_RESULT_RETURNED));
      if(bAddToContactUpdatedList) {
        contactImplBackground->setContactUpdateResultField(contactImplBackground->getContactUpdateResultField() | UPDATE_CONTACT_RESULT_NOTIFIED);
        contactImplBackground->calculateFullHash();
      }
      contactImplNotify = boost::make_shared<ContactImpl>(contactImplBackground.get());
      added_contacts->push_back(ContactPointer(new Contact(contactImplNotify)));
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " added: " << contactImplNotify->getDisplayName() << ", " << contactImplNotify->getHash());
    } else if(contactImplBackground->getFullHash() != it->second->getFullHash()) {
      if (contactImplBackground->getContactUpdateResultField() != it->second->getContactUpdateResultField()) {
        SG_ASSERT((contactImplBackground->getContactUpdateResultField() & UPDATE_CONTACT_RESULT_NOTIFIED) == 0 &&
                  (contactImplBackground->getContactUpdateResultField() & UPDATE_CONTACT_RESULT_RETURNED));
        contactImplBackground->setContactUpdateResultField(contactImplBackground->getContactUpdateResultField() | UPDATE_CONTACT_RESULT_NOTIFIED);
        contactImplBackground->calculateFullHash();
        bAddToContactUpdatedList = true;
      }
      contactImplNotify = boost::make_shared<ContactImpl>(contactImplBackground.get());
      changed_contacts->push_back(ContactPointer(new Contact(contactImplNotify)));
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " changed: " << contactImplNotify->getDisplayName() << ", " << contactImplNotify->getHash());
    }

    // snapshot
    new_lastNotifyContacts[contactImplNotify->getHash()] = contactImplNotify;
    indexContactByPhoneNumber(contactImplNotify, &new_phonenumbertoContact);
    if(!contactImplNotify->isDeleted() && !contactImplNotify->getAccountId().empty()) {
      new_tangoContactsForQuery[contactImplNotify->getAccountId()].push_back(contactImplNotify);
    } 

    // update/timeout list
    if (bAddToContactUpdatedList) {
      if (contactImplBackground->getContactUpdateResultField() & UPDATE_CONTACT_RESULT_TIMED_OUT) {
        SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " timeout: " << contactImplNotify->getDisplayName() << ", " << contactImplNotify->getHash());
        contactUpdatedTimedOutContacts->push_back(ContactPointer(new Contact(contactImplNotify)));
      } else {
        SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " updated: " << contactImplNotify->getDisplayName() << ", " << contactImplNotify->getHash());
        contactUpdatedSucceedContacts->push_back(ContactPointer(new Contact(contactImplNotify)));
      }
    }
  }
  
  // removed
  BOOST_FOREACH(const ContactImplMap::value_type& entry, lastNotifyContacts) {
    if(!contains(m_addressBookContacts, (entry.second)->getHash())) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " removed: " << (entry.second)->getDisplayName() << ", " << (entry.second)->getSid());
      removed_contacts->push_back(ContactPointer(new Contact(boost::make_shared<ContactImpl>(entry.second.get()))));
    }
  }

  // snapshot
  {
    sgiggle::pr::scoped_wlock guard(m_query_rwmutex.wmutex());
    m_lastNotifyContacts.swap(new_lastNotifyContacts);
    m_tangoContactsForQuery.swap(new_tangoContactsForQuery);
    m_phonenumbertoContact.swap(new_phonenumbertoContact);
  }

  // check
  {
    sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
    assertTangoContacts_();
  }

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " ends");
}

void ContactManager::cleanupAfterMigration__()
{
  SGLOG_DEBUG(__FUNCTION__);
  if (!useCRC32Hash__())
  {
    return ;
  }
  vector<std::string> contactsToClean;
  for (ContactImplIterator iter = m_addressBookContacts.begin() ; iter != m_addressBookContacts.end() ;) {
    if (iter->second->isUnderHashMigration__()) {
      SGLOG_DEBUG(__FUNCTION__<<" :remove transitional contact after contact filter "<< iter->second->getHash() << " with display name "<<iter->second->getDisplayName());
      contactsToClean.push_back(iter->second->getHash());
    }
    ++iter;
  }
  
  BOOST_FOREACH(const std::string& hash, contactsToClean) {
    // clean up from m_tangoContacts
    if(contains(m_tangoContacts, m_addressBookContacts[hash]->getAccountId())) {
      LETREF(vec, m_tangoContacts[m_addressBookContacts[hash]->getAccountId()]);
      foreach(it, vec) {
        if((*it)->getHash() == hash) {
          vec.erase(it);
          break;
        }
      }
    }
    // from addressbook
    m_addressBookContacts.erase(hash);
  }
}

/**
 *  batch the request in 5 seconds.
 *
 *  Do the following to reduce the probability that the subtle deadlock occurs.
 *
 *  - Avoid using lock during the whole life time of notifyServiceCallbacks_
 *    Solution: Create the copy of serviceCallbacks collection. Iterate over
 *    this copy to do notification instead of the original collection.
 *
 *  - Handle the potential use of lock in the caller of notifyCallbacks
 *    Solution: spraw the individual callback on the separate thread. It makes
 *    notifyServiceCallbacks_() return soon, which releases
 *    the lock in the caller of notifyServiceCallbacks_().
**/
void ContactManager::notifyServiceCallbacks_(bool bDoImmediately)
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD2(DispatcherThread::get_timer_dispatcher(), notifyServiceCallbacks_, bDoImmediately);

  batchedRequest(m_notifyCallbackTimerId, notifyServiceCallbacks_, bDoImmediately);
  
  sgiggle::corefacade::contacts::ContactVectorPointer added_contacts(new sgiggle::corefacade::contacts::ContactVector);
  sgiggle::corefacade::contacts::ContactVectorPointer changed_contacts(new sgiggle::corefacade::contacts::ContactVector);
  sgiggle::corefacade::contacts::ContactVectorPointer removed_contacts(new sgiggle::corefacade::contacts::ContactVector);
  sgiggle::corefacade::contacts::ContactVectorPointer contactUpdatedSucceedList(new sgiggle::corefacade::contacts::ContactVector);
  sgiggle::corefacade::contacts::ContactVectorPointer contactUpdatedTimedOutList(new sgiggle::corefacade::contacts::ContactVector);
  bool bOrderDisplayChanged = false;
  bool bToNotifyContactChangedCallback = false;

  {
    sgiggle::pr::scoped_lock lock(m_mutex);

    calculateContactsDifference_(added_contacts, changed_contacts, removed_contacts, contactUpdatedSucceedList, contactUpdatedTimedOutList);
    if (added_contacts->size() + changed_contacts->size() + removed_contacts->size() != 0) {
      bToNotifyContactChangedCallback = true;
    }

    if (m_notified_sort_order != m_sort_order || m_notified_display_order != m_display_order) {
      bOrderDisplayChanged = true;
    }

    if (bToNotifyContactChangedCallback || bOrderDisplayChanged) {
      ContactImplVector sortedList;
      ContactImplMap contacts;
      {
        sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
        contacts = m_lastNotifyContacts;
      }
      { // This sort function could take significantly long time on non-English contacts (e.g., Chinese ones), so we need to unlock this one
        sgiggle::pr::scoped_unlock unguard(m_mutex);
        sortedList = sortByCollation(contacts);
      }
      // sortByCollation also filter not-to-be-displayed contacts besides sorting.
      // we need to filter those contacts from contactXXXList here
      SEQ_ERASE(iter, *contactUpdatedSucceedList, (*iter)->isNotToBeDisplayed());
      SEQ_ERASE(iter, *contactUpdatedTimedOutList, (*iter)->isNotToBeDisplayed());
      {
        sgiggle::pr::scoped_wlock guard(m_query_rwmutex.wmutex());
        m_addressBookSortedList.swap(sortedList);
        m_notified_sort_order = m_sort_order;
        m_notified_display_order = m_display_order;
      }
      
      calculateHighlightedContactList();
      calculateRecommendationList();
    }
    
    if (bToNotifyContactChangedCallback) {
      saveTangoContactsToLocalStorage_();
    }
    
    if (m_bPstnRuleSet && !m_bPstnRuleSetNotify) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", set m_bPstnRuleSetNotify to true");
      m_bPstnRuleSetNotify = m_bPstnRuleSet;
      bToNotifyContactChangedCallback = true;
    }

    bToNotifyContactChangedCallback |= bOrderDisplayChanged;
    bToNotifyContactChangedCallback |= bDoImmediately;
  }

  if (bToNotifyContactChangedCallback) {
    notifyServiceCallbacks__(added_contacts, changed_contacts, removed_contacts, bOrderDisplayChanged, contactUpdatedSucceedList, contactUpdatedTimedOutList);
  }
}

void ContactManager::notifyServiceCallbacks__(const sgiggle::corefacade::contacts::ContactVectorPointer added_contacts,
    const sgiggle::corefacade::contacts::ContactVectorPointer changed_contacts,
    const sgiggle::corefacade::contacts::ContactVectorPointer removed_contacts,
    bool bOrderDisplayChanged,
    const sgiggle::corefacade::contacts::ContactVectorPointer contactUpdatedSucceedList,
    const sgiggle::corefacade::contacts::ContactVectorPointer contactUpdatedTimedOutList)
{
  std::map<int, ContactUpdateCallback> cbs;
  {
    sgiggle::pr::scoped_lock lock(m_quick_mutex);
    cbs = m_serviceUpdateCallbacks;
  }

  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", notify contact changed, added=" << added_contacts->size() << ", changed=" << changed_contacts->size() << ", removed=" << removed_contacts->size() << ", callbacks=" << cbs.size());
  BOOST_FOREACH(const sgiggle::corefacade::contacts::ContactPointer& contact, *added_contacts)
  {
    SGLOG_TRACE("ContactManager::"<<__FUNCTION__<<"   added=" 
        << contact->getDisplayName() << "(" << contact->getHash() << ")");
  }
  BOOST_FOREACH(const sgiggle::corefacade::contacts::ContactPointer& contact, *changed_contacts)
  {
    SGLOG_TRACE("ContactManager::"<<__FUNCTION__<<"   changed=" 
        << contact->getDisplayName()<< "(" << contact->getHash() << ")");
  }
  BOOST_FOREACH(const sgiggle::corefacade::contacts::ContactPointer& contact, *removed_contacts)
  {
    SGLOG_TRACE("ContactManager::"<<__FUNCTION__<<"   removed=" 
        << contact->getDisplayName() << "(" << contact->getHash() << ")");
  }
  BOOST_FOREACH(const sgiggle::corefacade::contacts::ContactPointer& contact, *contactUpdatedSucceedList)
  {
    SGLOG_TRACE("ContactManager::"<<__FUNCTION__<<"   updated=" 
        << contact->getDisplayName()<< "(" << contact->getHash() << ")" );
  }
  BOOST_FOREACH(const sgiggle::corefacade::contacts::ContactPointer& contact, *contactUpdatedTimedOutList)
  {
    SGLOG_TRACE("ContactManager::"<<__FUNCTION__<<"   timeout=" 
        << contact->getDisplayName() << "(" << contact->getHash() << ")");
  }

  for (std::map<int, ContactUpdateCallback>::const_iterator cit = cbs.begin(); cit != cbs.end(); ++cit) {
    if (m_isStopping) {
      break;
    }

    cit->second(added_contacts, changed_contacts, removed_contacts, bOrderDisplayChanged);
  }

  if (contactUpdatedSucceedList->size() > 0 || contactUpdatedTimedOutList->size() > 0) {
    UpdateContactResponseCallback updateContactResponseCallback;
    {
      sgiggle::pr::scoped_lock lock(m_quick_mutex);
      updateContactResponseCallback = m_updateContactResponseCallback;
    }
    if (updateContactResponseCallback)  {
      if (contactUpdatedSucceedList->size() > 0) {
        updateContactResponseCallback(contactUpdatedSucceedList, sgiggle::corefacade::contacts::ANCR_SUCCEED);
      }
      if (contactUpdatedTimedOutList->size() > 0) {
        updateContactResponseCallback(contactUpdatedTimedOutList, sgiggle::corefacade::contacts::ANCR_TIMED_OUT);
      }
    } else {
      SGLOG_TRACE("ContactManager::"<<__FUNCTION__<< ", no update contact response call back registered");
    }
  }
}

void ContactManager::notifyRecommendationCallbacks_()
{
  std::map<int, ContactRecommendationCallback> cbs;
  {
    sgiggle::pr::scoped_lock lock(m_quick_mutex);
    cbs = m_recommendationCallbacks;
  }

  for (std::map<int, ContactRecommendationCallback>::const_iterator cit = cbs.begin(); cit != cbs.end(); ++cit) {
    if (m_isStopping) {
      break;
    }

    cit->second();
  }
}

void ContactManager::inviteContactsBySMS(sgiggle::corefacade::contacts::PhoneNumberVectorPointer recipients, const std::string& message, int64_t inviteId)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", sz=" << (recipients ? recipients->size() : 0) << ", inviteId=" << inviteId);

  if (!recipients)
    return;

  sgiggle::telephony::Telephony *telephony = sgiggle::telephony::Telephony::getDriver();
  SG_ASSERT(telephony);

  int task_id = telephony->send_batch_sms(recipients, message, boost::bind(&ContactManager::onSendSMSFinished, this, _1, _2, _3, message, inviteId));
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", return task_id=" << task_id);
}

void ContactManager::onSendSMSFinished(int task_id, sgiggle::corefacade::contacts::PhoneNumberVectorPointer succeeded_list, sgiggle::corefacade::contacts::PhoneNumberVectorPointer failed_list, const std::string& message, int64_t inviteId)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", taskid=" << task_id << ", succeed=" << (succeeded_list ? succeeded_list->size() : 0) << ", failed=" << (failed_list ? failed_list->size() : 0) << ", inviteId=" << inviteId);

  onContactsInvitedBySMS(succeeded_list, failed_list, message, inviteId);
}

void ContactManager::onContactsInvitedByEmail(sgiggle::corefacade::util::StringVectorPointer succeeded_list, sgiggle::corefacade::util::StringVectorPointer failed_list,
                                              const std::string& subject, const std::string& message, int64_t inviteId)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", succeed=" << (succeeded_list ? succeeded_list->size() : 0) << ", failed=" << (failed_list ? failed_list->size() : 0) << ", inviteId=" << inviteId);

  if (!succeeded_list)
    return;

  uint64_t now = pr::time_val::now().to_uint64();

  {
    sgiggle::pr::scoped_lock guard(m_mutex);

    BOOST_FOREACH(ContactImplMap::value_type& entry, m_addressBookContacts) {
      if (entry.second->isDeleted())
        continue;

      for (size_t i = 0; i < entry.second->getEmailSize(); i++) {
        std::string email = entry.second->getEmailAt(i);
        if (std::find(succeeded_list->begin(), succeeded_list->end(), email) != succeeded_list->end()) {
       	  std::vector<std::string> email_hashes;
       	  PhoneNumTypeHashesMap pn_hashes;
          getAllSidsOfOneCompound(entry.second->getCompoundId(), entry.second->getAccountId(), email_hashes, pn_hashes);
          BOOST_FOREACH(const PhoneNumTypeHashesMap::value_type& entry2, pn_hashes) {
            BOOST_FOREACH(const std::string& hash, entry2.second) {
              ContactImplPointer c = m_addressBookContacts[hash];
              SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", set invite time to NOW, phoneNumber=" << c->getSid() << ", contact=" << c->getDisplayName());
              c->setInviteTime(now);
              c->setContactChanged();
            }
          }
          BOOST_FOREACH(const std::string& hash, email_hashes) {
            ContactImplPointer c = m_addressBookContacts[hash];
            SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", set invite time to NOW, email=" << c->getSid() << ", contact=" << c->getDisplayName());
            c->setInviteTime(now);
            c->setContactChanged();
          }
          break;
        }
      }
    }
  }

  notifyServiceCallbacks_();

  reportContactsInvitedByEmail(succeeded_list, failed_list, subject, message, inviteId);
}

void ContactManager::onContactsInvitedBySMS(sgiggle::corefacade::contacts::PhoneNumberVectorPointer succeeded_list, sgiggle::corefacade::contacts::PhoneNumberVectorPointer failed_list,
                                            const std::string& message, int64_t inviteId)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", succeed=" << (succeeded_list ? succeeded_list->size() : 0) << ", failed=" << (failed_list ? failed_list->size() : 0) << ", inviteId=" << inviteId);

  if (!succeeded_list)
    return;

  std::vector<std::string> hash_vec;
  {
    BOOST_FOREACH(const sgiggle::corefacade::contacts::PhoneNumberPointer& pnp, *succeeded_list) {
      if (!pnp)
        continue;

      BOOST_FOREACH(ContactImplPointer contactImpl, getContactsBySubscriberNumber(pnp->subscriberNumber)) {
        std::vector<std::string> email_hashes;
        PhoneNumTypeHashesMap pn_hashes;
        getAllSidsOfOneCompound(contactImpl->getCompoundId(), contactImpl->getAccountId(), email_hashes, pn_hashes);
        BOOST_FOREACH(const PhoneNumTypeHashesMap::value_type& entry2, pn_hashes) {
          BOOST_FOREACH(const std::string& hash, entry2.second)  {
            hash_vec.push_back(hash);
          }
        }
        BOOST_FOREACH(const std::string& hash, email_hashes) {
          hash_vec.push_back(hash);
        }
      }
    }
  }

  uint64_t now = pr::time_val::now().to_uint64();
  {
    sgiggle::pr::scoped_lock guard(m_mutex);
    BOOST_FOREACH(std::string& hash, hash_vec) {
      ContactImplMap::iterator it = m_addressBookContacts.find(hash);
      if (it != m_addressBookContacts.end()) {
        SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", set invite time to NOW, sid=" << it->second->getSid() << ", contact=" << it->second->getDisplayName());
        it->second->setInviteTime(now);
        it->second->setContactChanged();
      }  else {
        SG_ASSERT(false);
      }
    }
  }

  notifyServiceCallbacks_();

  reportContactsInvitedBySMS(succeeded_list, failed_list, message, inviteId);
}

void ContactManager::reportContactsInvitedByEmail(sgiggle::corefacade::util::StringVectorPointer succeeded_list, sgiggle::corefacade::util::StringVectorPointer failed_list,
                                                  const std::string& subject, const std::string& message, int64_t inviteId)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", succeed=" << (succeeded_list ? succeeded_list->size() : 0) << ", failed=" << (failed_list ? failed_list->size() : 0) << ", inviteId=" << inviteId << ", email_invite_type=" << tango::util::getDevInfoDriver()->email_invite_type());

  if (!succeeded_list)
    return;

  sgiggle::xmpp::SendInviteData::pointer data(new sgiggle::xmpp::SendInviteData(0,
    (tango::util::getDevInfoDriver()->email_invite_type() == sgiggle::init::DevInfo::DT_INVITE_CLIENT ? sgiggle::messages::INVITE_SEND_TYPE_EMAIL : sgiggle::messages::INVITE_SEND_TYPE_SERVER)));

  int contacts_suggested = 0;
  {
    sgiggle::pr::scoped_lock guard(m_mutex);

    BOOST_FOREACH(ContactImplMap::value_type& entry, m_addressBookContacts)
    {
      if (entry.second->isDeleted())
        continue;
      for (size_t i = 0; i < entry.second->getEmailSize(); i++) {
        ContactImplPointer contactImpl = entry.second;
        std::string email = contactImpl->getEmailAt(i);
        if (std::find(succeeded_list->begin(), succeeded_list->end(), email) != succeeded_list->end()) {
       	  std::vector<std::string> email_hashes;
         	PhoneNumTypeHashesMap pn_hashes;
          getAllSidsOfOneCompound(entry.second->getCompoundId(), entry.second->getAccountId(), email_hashes, pn_hashes);
          std::string pn;
          if (!pn_hashes.empty()) {
            std::string hash = pn_hashes.begin()->second[0];
            ContactImplPointer c = m_addressBookContacts[hash];
            pn = c->getSid();
          }
          SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", add to send report, displayName=" << contactImpl->getDisplayName() << ", email=" << email << ", pn=" << pn);
          data->addInvitee(contactImpl->getNamePrefix(), contactImpl->getFirstName(), contactImpl->getMiddleName(), contactImpl->getLastName(),
                           contactImpl->getNameSuffix(), contactImpl->getDisplayName(), email, pn);
          if (isInRecommendationList(contactImpl->getHash()))
            contacts_suggested++;
          break;
        }
      }
    }
  }

  data->setMessageBody(message);
  data->setInviteId(inviteId);

  sgiggle::xmpp::invite_request::pointer invite(new sgiggle::xmpp::invite_request(sgiggle::xmpp::invite_request::get_url(),
      sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), data));
  invite->send();

  sendLogToServer(inviteId, "Email", succeeded_list->size() + (failed_list ? failed_list->size() : 0), succeeded_list->size(), contacts_suggested);
}

void ContactManager::reportContactsInvitedBySMS(sgiggle::corefacade::contacts::PhoneNumberVectorPointer succeeded_list, sgiggle::corefacade::contacts::PhoneNumberVectorPointer failed_list,
                                                const std::string& message, int64_t inviteId)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", succeed=" << (succeeded_list ? succeeded_list->size() : 0) << ", failed=" << (failed_list ? failed_list->size() : 0) << ", inviteId=" << inviteId << ", sms_invite_type=" << tango::util::getDevInfoDriver()->sms_invite_type());

  if (!succeeded_list)
    return;

  sgiggle::xmpp::SendInviteData::pointer data(new sgiggle::xmpp::SendInviteData(0,
    (tango::util::getDevInfoDriver()->sms_invite_type() == sgiggle::init::DevInfo::DT_INVITE_CLIENT ? sgiggle::messages::INVITE_SEND_TYPE_SMS : sgiggle::messages::INVITE_SEND_TYPE_SERVER)));

  int contacts_suggested = 0;
  {
    BOOST_FOREACH(const sgiggle::corefacade::contacts::PhoneNumberPointer& pnp, *succeeded_list) {
      if (!pnp)
        continue;

      BOOST_FOREACH(ContactImplPointer contactImpl, getContactsBySubscriberNumber(pnp->subscriberNumber)) {
        std::vector<std::string> email_hashes;
        PhoneNumTypeHashesMap pn_hashes;
        getAllSidsOfOneCompound(contactImpl->getCompoundId(), contactImpl->getAccountId(), email_hashes, pn_hashes);
        std::string inviteeEmail = contactImpl->getEmailAt(0);
        if (!email_hashes.empty()) {
          std::string hash = email_hashes[0];
          sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());
          ContactImplMap::const_iterator it = m_lastNotifyContacts.find(hash);
          if (it != m_lastNotifyContacts.end()) {
            inviteeEmail = it->second->getSid();
          }
        }
        data->addInvitee(contactImpl->getNamePrefix(), contactImpl->getFirstName(), contactImpl->getMiddleName(), contactImpl->getLastName(),
                         contactImpl->getNameSuffix(), contactImpl->getDisplayName(), inviteeEmail, pnp->subscriberNumber);
        if (isInRecommendationList(contactImpl->getHash()))
          contacts_suggested++;
        break;
      }
    }
  }

  data->setMessageBody(message);
  data->setInviteId(inviteId);

  sgiggle::xmpp::invite_request::pointer invite(new sgiggle::xmpp::invite_request(sgiggle::xmpp::invite_request::get_url(),
      sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), data));
  invite->send();

  sendLogToServer(inviteId, "SMS", succeeded_list->size() + (failed_list ? failed_list->size() : 0), succeeded_list->size(), contacts_suggested);
}

void ContactManager::sendLogToServer(int64_t inviteId, const std::string& inviteType, int total, int succeeded, int contacts_suggested)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", inviteType=" << inviteType << ", total=" << total << ", succeed=" << succeeded << ", inviteId=" << inviteId);

  std::string log_str;
  log_str += "invite_event_type=invitation_sent&";
  log_str += "invite_id=" + sgiggle::to_string(inviteId) + "&";
  log_str += "invitation_type=" + inviteType + "&";
  log_str += "contacts_selected=" + sgiggle::to_string(total) + "&";
  log_str += "contacts_suggested=" + sgiggle::to_string(contacts_suggested) + "&";
  log_str += "recommendation_list_size=" + sgiggle::to_string(m_recommendationList->size()) + "&";
  log_str += "invitations_sent=" + sgiggle::to_string(succeeded);
  stats_collector::singleton()->log_to_server(sgiggle::stats_collector::LOG_INFO, log_str);
}

/*
 * get all sids of a compound in which the given seed_sid locates.
 * this is used in invitation, since it's rarely used, so low efficiency is bearable
 * accountId of tango registered user should not be passed as parameter. so account_id is either an ATM one or empty
 * if accountId is empty, the compound is not associated with any account_id, return all sids of the compound
 * otherwise, if it's associated with more than one account_id, then all non tango emails and phone numbers belongs to the smallest account_id.
 */
void ContactManager::getAllSidsOfOneCompound(const std::string& compoundId, const std::string& account_id, std::vector<std::string>& email_hashes, PhoneNumTypeHashesMap& pn_hashes) const
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", compoundId=" << compoundId << ", accountId=" << account_id);
  sgiggle::pr::scoped_rlock guard(m_query_rwmutex.rmutex());

  bool bSMSAvailable = isSMSAvailable();
  bool bEmailAvailable = isEmailAvailable();

  std::vector<std::string> temp_email_hashes;
  PhoneNumTypeHashesMap temp_pn_hashes;
  bool bNonTangoSidsBelongToOtherContact = false;

  ContactImplMap::const_iterator it;

  for (it = m_lastNotifyContacts.begin(); it != m_lastNotifyContacts.end(); it++) {
    if (it->second->isDeleted())
      continue;

    if (it->second->getCompoundId() != compoundId)
      continue;

    if (account_id.empty()) {
      if (!it->second->getAccountId().empty()) {
        bNonTangoSidsBelongToOtherContact = true;
      }
    } else if (!it->second->getAccountId().empty() && it->second->getAccountId() < account_id) {
      bNonTangoSidsBelongToOtherContact = true;
    }

    if (bSMSAvailable && it->second->getPhoneNumberSize() > 0) {
      PhoneNumber pn = it->second->getPhoneNumbers()[0];
      if (it->second->getAccountId().empty()) {
        temp_pn_hashes[pn.getType()].push_back(it->first);
      } else if (it->second->getAccountId() == account_id) {
        pn_hashes[pn.getType()].push_back(it->first);
        SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", add phone hash " << it->first);
      }
    }
    if (bEmailAvailable && it->second->getEmailSize() > 0)
    {
      if (it->second->getAccountId().empty()) {
        temp_email_hashes.push_back(it->first);
      } else if (it->second->getAccountId() == account_id) {
        email_hashes.push_back(it->first);
        SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", add email hash " << it->first);
      }
    }
  }

  if (!bNonTangoSidsBelongToOtherContact)
  {
    BOOST_FOREACH(const std::string& hash, temp_email_hashes) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", add non tango email hash " << hash);
      email_hashes.push_back(hash);
    }
    BOOST_FOREACH(PhoneNumTypeHashesMap::value_type& entry, temp_pn_hashes) {
      BOOST_FOREACH(const std::string& hash, entry.second) {
        SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", add non tango phone hash " << hash);
        pn_hashes[entry.first].push_back(hash);
      }
    }
  }
}

/*
 * called in addContact(), addContactFromUpdate(), commitAddressBookSync()
 */
void ContactManager::indexContactByPhoneNumber(ContactImplPointer contactImpl, PhoneNumberContactMultiMap* pMap)
{
  if(!pMap) {
    pMap = &m_phonenumbertoContact;
  }
  BOOST_FOREACH(PhoneNumber pn, contactImpl->getPhoneNumbers()) {
    std::string subscriber_number = pn.getSubscriberNumber();
    std::string key = (subscriber_number.length() < NUMBER_OF_DIGITS_AS_KEY_FOR_PHONE_NUMBER ?
                       subscriber_number :
                       subscriber_number.substr(subscriber_number.length() - NUMBER_OF_DIGITS_AS_KEY_FOR_PHONE_NUMBER));

    BOOST_FOREACH(const ContactImplPointer& c, (*pMap)[key]) {
      if (c->getHash() == contactImpl->getHash()) {
        return;
      }
    }
    (*pMap)[key].push_back(contactImpl);
  }
}

//////////////////////////////////////////////////////////////////
//#pragma mark - Contact Filter
//////////////////////////////////////////////////////////////////

// timeout for ContactResolve
void ContactManager::contactResolveTimeout_(const std::string& session_id)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);
  { // atomicity with execContactUpdateTask_
    pr::scoped_lock lock(m_mutex);
    if(session_id.empty() || session_id == m_contactFilterSessionId) {
      cancelContactResolve();
    }
  }
}

void ContactManager::cancelContactResolveTimer_()
{
  if (m_contactResolveTimer != -1) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": cancel contact resolver timer");
    sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor()->CancelTimer(m_contactResolveTimer);
    m_contactResolveTimer = -1;
  }
}

void ContactManager::rescheduleContactResolveTimer(const std::string& session_id)
{
  pr::scoped_lock guard(m_mutex);
  
  if (!session_id.empty() && session_id != m_contactFilterSessionId) {
    SGLOG_DEBUG("ContactManager::"<<__FUNCTION__<<", bad session id, skip");
    return;
  }
  
  cancelContactResolveTimer_();
  
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": reschedule contact resolver timer");
  m_contactResolveTimer = sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor()->SetTimer(CONTACT_RESOLVE_TIMEOUT,
                                                                                                     boost::bind(&ContactManager::contactResolveTimeout_, this, session_id));
}

const bool ContactManager::isContactResolveFinished() const
{
  pr::scoped_lock lock(m_mutex); // for atomic
  return m_ContactResolveFinished;
}

bool ContactManager::isContactFilterUploaded(const std::string& session_id) const
{
  sgiggle::pr::scoped_lock lock(m_mutex);
  
  if (session_id != m_contactFilterSessionId) {
    SGLOG_DEBUG("ContactManager::"<<__FUNCTION__<<", bad session id, skip");
    return false;
  }
  
  return m_current_it == m_addressBookContacts.end();
}

bool ContactManager::isContactFilterDoneForTheFirstTime() const
{
  sgiggle::pr::scoped_lock lock(m_mutex);
  return m_1stTimeContactFilterSucceeded;
}

/*
 * called in xmpp/XmppSessionImpl.cpp
 */
void ContactManager::resetContactFilteringCurrentSessionFinished()
{
}

/*
 * called in facilitator_request/contact_filter_request.cpp
 * to try to get next 200 records
 */
bool ContactManager::adaptAddressBookContactsNextRound(ContactAdapterInterface& adapter,
                                                       std::string& sessionID,
                                                       bool& multiGroup,
                                                       bool& moreGroup,
                                                       uint32_t& seq,
                                                       size_t max_contacts)
{
  sgiggle::pr::scoped_lock lock(m_mutex);
  
  seq = m_seq_number++;
  sessionID = m_contactFilterSessionId;
  multiGroup = m_addressBookContacts.size() > max_contacts;
  size_t packedContacts = 0;
  for (; m_current_it != m_addressBookContacts.end() && packedContacts < max_contacts; m_current_it++) {
    if (m_current_it->second->isFromAddressbook() && !m_current_it->second->isDeleted()) {
      adapter.adapt(m_current_it->second);
      packedContacts++;
    }
  }
  moreGroup = m_current_it != m_addressBookContacts.end();
  
  return (packedContacts > 0);
}

void ContactManager::cancelContactResolve()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ );

  pr::scoped_lock lock(m_mutex);

  if (m_contact_request) {
    SGLOG_DEBUG("ContactManager::"<<__FUNCTION__<<" cancel contact request ");
    m_contact_request->cancel();
    m_contact_request.reset();
  }

  setContactResolveFinished_();
}

void ContactManager::setContactResolveFinished_()
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ );
  
  cancelContactResolveTimer_();
  setState_(CONTACTMGR_STATE_ADDRESS_BOOK_LOADED);
  m_ContactResolveFinished = true;
}

void ContactManager::resetContactFiltering_()
{
  cancelContactResolve();
  SGLOG_DEBUG("ContactManager::"<<__FUNCTION__);

  // so that before resetSessionId the callback can still run
  m_filterResultMap.clear();

  resetContactFilterSessionId_();
  m_ContactResolveFinished = false;
  m_addressBookHashBasedOnWhichContactsAreFilteredForCurrentCycle = m_addressBookContactsOverallHash;
  m_countrycodeBasedOnWhichContactsAreFilteredForCurrentCycle = sgiggle::xmpp::UserInfo::getInstance()->countryid();

  setState_(CONTACTMGR_STATE_CONTACT_FILTERING);
}

void ContactManager::setNeedToContactUpdate(boost::optional<com::tango::facilitator::client::proto::v3::filteraccount::ContactFilteringCauseEnum> cause)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);
  
  {
    pr::scoped_lock guard(m_quick_mutex);
    if (m_FgDeferContactUpdateTimerId != -1) {
      xmpp::MediaEngineManager::getInstance()->getProcessorImpl()->CancelTimer(m_FgDeferContactUpdateTimerId);
      m_FgDeferContactUpdateTimerId = -1;
    }
  }
  
  prepareToContactUpdate(false, cause);
}

void ContactManager::setNeedToContactFilter(boost::optional<v3::filteraccount::ContactFilteringCauseEnum> cause, const std::string& acme_id)
{
  SG_ASSERT(cause);
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__<<" reason " << *cause);
  
  if(cause == v3::filteraccount::SERVER) {
    SG_ASSERT(!acme_id.empty());
    m_contactfiltering_acme_id = acme_id;
  }
  prepareToContactUpdate(true, cause);
}

void ContactManager::prepareToContactUpdate(bool bContactFilter, boost::optional<v3::filteraccount::ContactFilteringCauseEnum> cause)
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD2(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessor(), prepareToContactUpdate, bContactFilter,cause);
  
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": bForce=" << bContactFilter << ", state=" << m_state << ", foreground=" << isUIInForeground() );
  
  pr::scoped_lock lock(m_mutex);
 
  // if contact filtering alread started, for registration triggered cf, don't do contact filtering if the country code is not changed.
  if (cause && cause == com::tango::facilitator::client::proto::v3::filteraccount::REGISTRATION) {
    if (m_state == CONTACTMGR_STATE_CONTACT_FILTERING) {
      if (m_countrycodeBasedOnWhichContactsAreFilteredForCurrentCycle == sgiggle::xmpp::UserInfo::getInstance()->countryid()) {
        SGLOG_DEBUG(__FUNCTION__<< "skip registration triggered contact filtering as country code is same as ongoing contact filtering. Continue on current task.");
        return;
      }
    } else {
      if (m_countrycodeBasedOnWhichContactsAreFilteredLastTime == sgiggle::xmpp::UserInfo::getInstance()->countryid()) {
        SGLOG_DEBUG(__FUNCTION__<< "skip registration triggered contact filtering as country code is same as last contact filtering");
        return;
      }
    }
  }
  
  if (bContactFilter) {
    m_bForceNotify = true;
    if (m_state == CONTACTMGR_STATE_CONTACT_FILTERING) {
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", cancel current contact filtering");
      cancelContactResolve();
    }
  }
  
  //We need to upload empty address book if the access is not allowed
  if (( m_state == CONTACTMGR_STATE_ADDRESS_BOOK_LOADED || m_state == CONTACTMGR_STATE_ADDRESS_BOOK_NOT_ALLOWED) && isUIInForeground()) {
    tryDoContactUpdateTask(bContactFilter,cause);
  } else {
    SGLOG_DEBUG(__FUNCTION__<<" m_state=" << getStateName(m_state) <<", UIInForeground=" << isUIInForeground() << " Don't start Contact Update Task");
  }
}

void ContactManager::tryDoContactUpdateTask(bool forceCF, boost::optional<v3::filteraccount::ContactFilteringCauseEnum> cause)
{
  sgiggle::xmpp::ProcessorImpl::pointer processor = sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessorImpl();
  
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " force contact filter "<< forceCF);
  
  // there's a timer, so lock.
  pr::scoped_lock guard(m_mutex);
  
  if (!isUIInForeground() || ( m_state != CONTACTMGR_STATE_ADDRESS_BOOK_LOADED && m_state != CONTACTMGR_STATE_ADDRESS_BOOK_NOT_ALLOWED) ) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", Refuse. in background or state=" << m_state);
    return;
  }

  if (m_addressBookSyncWaitTimerId != -1) {
    processor->CancelTimer(m_addressBookSyncWaitTimerId);
    m_addressBookSyncWaitTimerId = -1;
  }

  if ( this->getTangoContactResolveType() == sgiggle::contacts::ContactStore::Address_Book_Sync ) {
    if ( this->isContactResolveFinished() && processor->getXmppClient()) {
      SGLOGF_DEBUG("Start addressbook sync");
      stats_collector::singleton()->append_info_with_timestamp("ab_sync_start");
      resetAddressbookSync_();
      sgiggle::xmpp::AddressBookSyncData* data = new sgiggle::xmpp::AddressBookSyncData(0, 0 ,0);
      talk_base::Task* task = new sgiggle::xmpp::AddressBookSyncTask(processor.get(), processor->getXmppClient(), data);
      delete data;
      task->Start();
    } else {
      SGLOGF_DEBUG("Addressbook syncing...");
      m_addressBookSyncWaitTimerId = processor->SetTimer(1000, boost::bind(&this_type::tryDoContactUpdateTask, this,forceCF,cause));
      return;
    }
  } else {
    if ((sgiggle::xmpp::UserInfo::getInstance()->countryid().empty() || sgiggle::xmpp::UserInfo::getInstance()->subscribernumber().empty())
        && sgiggle::xmpp::UserInfo::getInstance()->email().empty()) {
      SGLOG_WARN("ContactManager::" << __FUNCTION__ << "%s: Neither phone number nor email registered yet. Ignore FILTER_CONTACT_TYPE.");
      return;
    }
    v3::filteraccount::ContactFilteringCauseEnum tmp_cause ;
    if (forceCF) {
      execContactUpdateTask_(tango::httpme::contact_filter_request::CONTACT_FILTER_REQUEST_TYPE, cause);
    } else if (needToUseFilterContactsRequest_(tmp_cause)) {
      execContactUpdateTask_(tango::httpme::contact_filter_request::CONTACT_FILTER_REQUEST_TYPE,tmp_cause);
    } else if (needToUseUploadContactsRequest_()) {
      execContactUpdateTask_(tango::httpme::upload_contacts_request::UPLOAD_CONTACTS_REQUEST_TYPE, cause);
    } else {
      m_1stTimeContactFilterSucceeded = true;
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ": do not need filter contacts, skip");
    }
  }
}

// pre: m_mutex
void ContactManager::execContactUpdateTask_(const std::string& type, boost::optional<v3::filteraccount::ContactFilteringCauseEnum> cause)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__<< " type: " << type );
  resetContactFiltering_();
  doContactUpdateOneMoreRound(m_contactFilterSessionId, type, cause);
}

/*
 * called locally in tryDoContactFilter()
 * called from facilitator_request::handle_response.
 */
void ContactManager::doContactUpdateOneMoreRound(const std::string& session_id,
                                                 const std::string& type,
                                                 boost::optional<v3::filteraccount::ContactFilteringCauseEnum> cause)
{
  sgiggle::xmpp::ProcessorImpl::pointer processor = sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessorImpl();
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD2(processor, doContactUpdateOneMoreRound, session_id, type, cause);
  
  if (m_contactFilterOneRoundInCallRetryTimerId != -1) {
    processor->CancelTimer(m_contactFilterOneRoundInCallRetryTimerId);
    m_contactFilterOneRoundInCallRetryTimerId = -1;
  }
  
  if (processor->inCall() || (!this->isAddressBookLoaded()) ) {
    SGLOG_DEBUG("ContactManager::" <<__FUNCTION__ << "Contact-filter delay (In-call or Address-Book not done loaded). Check back in 1-sec.");
    m_contactFilterOneRoundInCallRetryTimerId = processor->SetTimer(1000, boost::bind(&this_type::doContactUpdateOneMoreRound, this, session_id, type, cause));
    return;
  }
  
  pr::scoped_lock g(m_mutex);
  
  if (m_contactFilterSessionId != session_id) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", bad session_id, skip.");
    return;
  }
  
  rescheduleContactResolveTimer();
  
  if (tango::httpme::contact_filter_request::CONTACT_FILTER_REQUEST_TYPE == type) {
    SGLOG_DEBUG("ContactManager::"<<__FUNCTION__ << ", CONTACT_FILTER_REQUEST_TYPE");
    m_contact_request = tango::httpme::contact_filter_request::create(tango::httpme::contact_filter_request::get_url(), processor, cause, m_contactfiltering_acme_id);
    m_contact_request->send();
  } else if (tango::httpme::upload_contacts_request::UPLOAD_CONTACTS_REQUEST_TYPE == type) {
    SGLOG_DEBUG("ContactManager::"<<__FUNCTION__ << ", CONTACT_FILTER_REQUEST_TYPE");
    m_contact_request = tango::httpme::upload_contacts_request::create (tango::httpme::upload_contacts_request::get_url(), processor);
    m_contact_request->send();
  } else {
    SG_ASSERT(false);
  }
}

void ContactManager::onAcmeContactFilterMessage(std::string id, std::string msg)
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD2(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessorImpl(), onAcmeContactFilterMessage, id, msg);

  bool cfViaACMEEnabled = sgiggle::server_owned_config::ServerOwnedConfigManager::getInstance()->get<bool>(ACME_TRIGGERED_CONTACT_FILTER_V2_ENABLED, false) ;
  SGLOG_DEBUG("ContactManager::" <<__FUNCTION__ << ", GOT ACME message with id: " << id
              << " Contact Filtering over ACME is: " << ((cfViaACMEEnabled)?"Enabled":"Disabled"));

  if (m_isStopping) {
    return;
  }

  if (!isUIInForeground()) {
    SGLOG_DEBUG("ContactManager::" <<__FUNCTION__ << ", GOT ACME message while in background.");
  }

  using com::tango::acme::proto::v1::message::refresh::RefreshMessageV1;
  RefreshMessageV1 refreshMessage;
  if (refreshMessage.ParseFromString(msg)) {
    SGLOG_DEBUG("ContactManager::" <<__FUNCTION__ << ", GOT ACME message with message: " << msg << ", service name: " << refreshMessage.servicename() << ", service identifier: " << refreshMessage.serviceidentifier());

    const std::string &serviceIdentifier = refreshMessage.serviceidentifier();
    if (CONTACTFILTERING_ACME_SERVICE_ID == serviceIdentifier) {
      if (cfViaACMEEnabled) {
        SGLOG_DEBUG(__FUNCTION__ << " Calling Contact Filtering");

        setNeedToContactFilter(v3::filteraccount::SERVER, id);
        
        SGLOG_DEBUG("ContactManager::" <<__FUNCTION__ << ", Trying to send test event : ACME_CONTACTFILTERING =  " << id);
        sgiggle::testing::send_test_event(std::make_pair(std::string("ACME_CONTACTFILTERING"), id));
      } else {
        SGLOG_ERROR(__FUNCTION__ << "unexpected service_id " << serviceIdentifier << " ACME CF v2 disabled");
        tango::acme::Acme::getInstance()->ack(id, false);
      }
    } else {
      SGLOG_ERROR(__FUNCTION__ << "unexpected service_id " << serviceIdentifier);
      tango::acme::Acme::getInstance()->ack(id, false);
    }
  }
}

void ContactManager::onAcmeGetTangoContactsMessage(std::string id, std::string msg)
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD2(sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessorImpl(), onAcmeGetTangoContactsMessage, id, msg);

  SGLOG_DEBUG("ContactManager::" <<__FUNCTION__ << ", GOT ACME message with id: " << id );
  if (!sgiggle::server_owned_config::ServerOwnedConfigManager::getInstance()->get<bool>(ACME_TRIGGERED_CONTACT_FILTER_V2_ENABLED, false) ) {
    SGLOG_DEBUG(__FUNCTION__<<" ACME CF V2 Disabled");
    return ;
  }

  if (m_isStopping) {
    return;
  }

  if(!isUIInForeground()) {
    SGLOG_DEBUG("ContactManager::" <<__FUNCTION__ << ", GOT ACME message while in background.");
  }

  using com::tango::acme::proto::v1::message::refresh::RefreshMessageV1;
  RefreshMessageV1 refreshMessage;
  if(refreshMessage.ParseFromString(msg)){
    SGLOG_DEBUG("ContactManager::" <<__FUNCTION__ << ", GOT ACME message with message: " << msg
                << ", service name: " << refreshMessage.servicename() << ", service identifier: " << refreshMessage.serviceidentifier());

    const std::string &serviceIdentifier = refreshMessage.serviceidentifier();
    if (GETTANGOCONTACTS_ACME_SERVICE_ID == serviceIdentifier){
      SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " Calling Refresh Tango Contacts ");

      sgiggle::xmpp::ProcessorImpl::pointer processor = sgiggle::xmpp::MediaEngineManager::getInstance()->getProcessorImpl();

      if ( processor->inCall() ) {
        SGLOG_DEBUG("ContactManager::" <<__FUNCTION__<<" In call, delay refresh_tango_contact request");
        tango::acme::Acme::getInstance()->ack(id,false);
        return ;
      }

      { // atomicity
        pr::scoped_lock guard(m_mutex);
        if (m_state == CONTACTMGR_STATE_CONTACT_FILTERING) {
          SGLOG_DEBUG("ContactManager::"<<__FUNCTION__ << " In Contact Filtering now");
          tango::acme::Acme::getInstance()->ack(id,false); // Can't handle this time, will be delievered again
          return ;
        }
        
        // Create refresh_Tango_contacts request:
        // input: acme id to ack
        // TODO: probably can be integrated to tryDoContactUpdateTask. Difference 
        // is that:
        //  - the refresh_tango_contacts_request doesn't doContactUpdateOneMoreRound
        resetContactFiltering_();
        rescheduleContactResolveTimer();

        m_contact_request = tango::httpme::refresh_tango_contacts_request::pointer(
            new tango::httpme::refresh_tango_contacts_request(id, processor, m_contactFilterSessionId));
        m_contact_request->send();
      }

      SGLOG_DEBUG("ContactManager::" <<__FUNCTION__ << ", Trying to send test event : ACME_GETANGOCONTACTS =  " << id);
      sgiggle::testing::send_test_event(std::make_pair(std::string("ACME_GETANGOCONTACTS"), id));
    }else{
      SGLOG_ERROR(__FUNCTION__ << "unexpected service_id " << serviceIdentifier);
      tango::acme::Acme::getInstance()->ack(id, false);
    }
  }
}

bool ContactManager::needToUseFilterContactsRequest_(com::tango::facilitator::client::proto::v3::filteraccount::ContactFilteringCauseEnum& cause) const
{
  if (!sgiggle::xmpp::UserInfo::getInstance()->allowStoreAddressBook()) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", not allow to store addressbook on server");
    cause = v3::filteraccount::OPT_OUT ;
    return true;
  }
  if (xmpp::UserInfo::getInstance()->getPersistentContactFilterVersion() < xmpp::UserInfo::PERSISTENT_CONTACT_UNENCODED_SID_HASH_VERSION ) {
    SGLOG_DEBUG("ContactManager::"<<__FUNCTION__<<" persistent contact filter version "<< xmpp::UserInfo::getInstance()->getPersistentContactFilterVersion() << " do contact filter");
    cause = v3::filteraccount::ACME_CF_V2_DISABLED ;
    return true;
  }
  if (!sgiggle::server_owned_config::ServerOwnedConfigManager::getInstance()->get<bool>(ACME_TRIGGERED_CONTACT_FILTER_V2_ENABLED, false) ) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", acme is disabled");
    cause = v3::filteraccount::ACME_CF_V2_DISABLED ;
    return true;
  }
  if (sgiggle::server_owned_config::ServerOwnedConfigManager::getInstance()->get<bool>(ACME_CONTACT_FILTER_V2_FALLBACK,true) &&
      addressBookNotSyncedWithServer_() ) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", Address book overall hash value has changed and Contact Filter fallback");
    cause = v3::filteraccount::FALLBACK;
    return true;
  }
  if (sgiggle::pr::time_val::now().to_uint64() - m_lastContactFilterTime >
    sgiggle::server_owned_config::ServerOwnedConfigManager::getInstance()->get<uint64_t>(CONTACT_FILTER_REFRESH_INTERVAL_KEY, DEFAULT_CONTACT_FILTER_REFRESH_MIN_PERIOD) * 1000) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", havn't done for "<< sgiggle::pr::time_val::now().to_uint64() - m_lastContactFilterTime);
    cause = v3::filteraccount::SCHEDULED ;
    return true;
  }
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " no need to contact filter request");
  return false;
}

bool ContactManager::needToUseUploadContactsRequest_() const
{
  if (!sgiggle::server_owned_config::ServerOwnedConfigManager::getInstance()->get<bool>(ACME_TRIGGERED_CONTACT_FILTER_V2_ENABLED, false)) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", acme is disabled");
    return false;
  }

  if (sgiggle::server_owned_config::ServerOwnedConfigManager::getInstance()->get<bool>(ACME_CONTACT_FILTER_V2_FALLBACK,true) ) {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", ACME FALLBACK is enabled");
    return false;
  }
  
  if (addressBookNotSyncedWithServer_()) {
    SGLOG_DEBUG(__FUNCTION__<<" Address book overall hash value has changed");
    return true;
  }
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << ", no need to upload.");
  return false;
}

bool ContactManager::addressBookNotSyncedWithServer_() const
{
  SGLOG_TRACE("ContactManager::" << __FUNCTION__);
  
  if (m_addressBookHashBasedOnWhichContactsAreFilteredLastTime == 0 ||
      m_addressBookHashBasedOnWhichContactsAreFilteredLastTime != m_addressBookContactsOverallHash ||
      m_countrycodeBasedOnWhichContactsAreFilteredLastTime != sgiggle::xmpp::UserInfo::getInstance()->countryid() )
  {
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " new overall hash "<< m_addressBookContactsOverallHash << " vs last time overall hash "<< m_addressBookHashBasedOnWhichContactsAreFilteredLastTime);
    SGLOG_DEBUG("ContactManager::" << __FUNCTION__ << " new user country code " << sgiggle::xmpp::UserInfo::getInstance()->countryid() << " vs last value " << m_countrycodeBasedOnWhichContactsAreFilteredLastTime);
    return true;
  }
  return false;
}

void ContactManager::addFilteredContactToBuffer(const std::string& session_id, const std::string& hash, const std::string& accountId,
    const std::map<std::string, std::string>& capabilities)
{

  FilterResultItem item;
  item.accountId = accountId;
  item.capabilities = capabilities;

  { // atomicity with execContactUpdateTask_
    pr::scoped_lock lock(m_mutex);
    if (m_contactFilterSessionId == session_id) {
      m_filterResultMap[hash] = item;
    }
  }
}

void ContactManager::handleUploadContactDone(const std::string& session_id)
{
  SGLOG_DEBUG("ContactManager::"<<__FUNCTION__);
  
  { // atomicity with execContactUpdateTask_
    pr::scoped_lock lock(m_mutex);

    if (session_id != m_contactFilterSessionId) {
      SGLOG_DEBUG("ContactManager::"<<__FUNCTION__<<", bad session id, skip");
      return;
    }
    setContactResolveFinished_();
    
    m_addressBookHashBasedOnWhichContactsAreFilteredLastTime = m_addressBookHashBasedOnWhichContactsAreFilteredForCurrentCycle;
    m_addressBookHashBasedOnWhichContactsAreFilteredForCurrentCycle = 0;
    m_countrycodeBasedOnWhichContactsAreFilteredLastTime = m_countrycodeBasedOnWhichContactsAreFilteredForCurrentCycle ;
    m_countrycodeBasedOnWhichContactsAreFilteredForCurrentCycle = "";
    sgiggle::xmpp::UserInfo::getInstance()->setAddressBookHashBasedOnWhichContactsAreFilteredLastTime(m_addressBookHashBasedOnWhichContactsAreFilteredLastTime);
    sgiggle::xmpp::UserInfo::getInstance()->setUserCountryCodeBasedOnWhichContactsAreFilteredLastTime(m_countrycodeBasedOnWhichContactsAreFilteredLastTime);
    SGLOG_DEBUG("ContactManager::"<<__FUNCTION__<<" overall hash saved "<<m_addressBookHashBasedOnWhichContactsAreFilteredLastTime);
  }
  sgiggle::xmpp::UserInfo::getInstance()->save();
}

void ContactManager::handleContactFilterFailed(const std::string& session_id)
{
  SGLOG_DEBUG("ContactManager::" << __FUNCTION__);

  { // atomicity with execContactUpdateTask_
    pr::scoped_lock lock(m_mutex);
    
    if (session_id != m_contactFilterSessionId) {
      SGLOG_DEBUG("ContactManager::"<<__FUNCTION__<<", bad session id, skip");
      return;
    }
    setContactResolveFinished_();
  }
}
