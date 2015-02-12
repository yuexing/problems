#include "ProfileSimpleFetcher.h"
#include "ProfileServiceImpl.h"
#include <boost/foreach.hpp>
#include <tango/util/app_state_mgr.h>
#include <tango/util/thread_util.h>
#include <tango/session/Utils.h>
#include <tango/phone_formatter/PhoneFormatter.h>

#define SGMODULE sgiggle::module::contacts

namespace sgiggle {
namespace social {

const char* const STORAGE_FILE_NAME = "cachedTangoFriends.dat";
sgiggle::pr::atomic<bool> ProfileSimpleFetcher::s_bRunning;

ProfileSimpleFetcher::ProfileSimpleFetcher() : m_mutex("ProfileSimpleFetcher", true)
{
  SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__);
  m_next_cb_id = 0;
  m_bCacheTangoFriendsChanged = false;
  m_backgrounded_handler_id = -1;
}

ProfileSimpleFetcher::~ProfileSimpleFetcher()
{

}

void ProfileSimpleFetcher::start()
{
  SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__);

  s_bRunning.set(true);
  loadFromCache();

  m_backgrounded_handler_id = tango::app_state_mgr::getInstance()->register_backgrounded_handler(boost::bind(&this_type::onAppBackground, this));
}

void ProfileSimpleFetcher::stop()
{
  SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__);

  s_bRunning.set(false);
  tango::app_state_mgr::getInstance()->remove_backgrounded_handler(m_backgrounded_handler_id);
}

void ProfileSimpleFetcher::onAppBackground()
{
  SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__);

  saveToCache();
}

int ProfileSimpleFetcher::registerContactChangedCallback(ProfileServiceContactChangedCallback callback)
{
  sgiggle::pr::scoped_lock lock(m_mutex);

  int cb_id = m_next_cb_id++;
  m_callback_map[cb_id] = callback;
  return cb_id;
}

void ProfileSimpleFetcher::unregisterContactChangedCallback(int cb_id)
{
  sgiggle::pr::scoped_lock lock(m_mutex);

  m_callback_map.erase(cb_id);
}

bool ProfileSimpleFetcher::addContact(const contacts::Contact& c, uint64_t server_time_in_ms, bool bAddIfNotExists)
{
  bool bChanged = false;
  contacts::ContactImplPointer contactImpl;
  { // atomically add
    sgiggle::pr::scoped_lock lock(m_mutex);
    
    PSFContactMap::iterator it = m_contact_map.find(c.getAccountId());
    if (it != m_contact_map.end()) {
      if (bAddIfNotExists) { // exist then don't add
        return false;
      }
      SGLOG_TRACE("ProfileSimpleFetcher::" << __FUNCTION__ << ", already found in cache, accountId=" << c.getAccountId());
    } else { // add a stub
      SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", add new, accountId=" << c.getAccountId() << ", displayName=" << c.getDisplayName());

      contacts::ContactImplPointer contactImpl = contacts::ContactImplPointer(new contacts::ContactImpl);
      contactImpl->setAccountId(c.getAccountId());
      contactImpl->setHash(contacts::ContactManager::composeHashByAccountIdForSocialOnly(c.getAccountId()));
      contactImpl->setThumbnailUrl(contacts::ContactImpl::INVALID_THUMBNAIL_URL);
      m_contact_map[c.getAccountId()] = contactImpl;
    }

    bChanged = modifyContactImpl(c, server_time_in_ms);
    contactImpl = m_contact_map[c.getAccountId()];
  }
  
  if(bChanged) {
    // update ContactManager
    contacts::Contact c(contactImpl);
    contacts::ContactManager::getInstance()->adaptSocialContact(c);
    // others
    notifyCallbacks(c.getAccountId());
  }
}

sgiggle::contacts::ContactImplPointer ProfileSimpleFetcher::getSocialContact(const std::string& accountId, PSFMode mode)
{
  SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", accountId=" << accountId << ", mode=" << mode);
  contacts::ContactImplPointer contactImpl;
  { // atomically set requesting
    sgiggle::pr::scoped_lock lock(m_mutex);

    PSFContactMap::iterator it = m_contact_map.find(accountId);
    if (it != m_contact_map.end()) {
      if (!it->second) { // contact has been deleted
        SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", contact deleted");
        return contacts::ContactImplPointer();
      }
      contactImpl = it->second;
    } else {
      contactImpl = contacts::ContactImplPointer(new contacts::ContactImpl);
      contactImpl->setAccountId(accountId);
      m_contact_map[accountId] = contactImpl;
    }

    if (mode == PSF_NO_FETCH || (contactImpl && contactImpl->isRequestingProfile())) {
      SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", return local result, mode=" << mode
                  << ", displayName=" << (contactImpl ? contactImpl->getDisplayName() : "") << ", requesting=" << (contactImpl && contactImpl->isRequestingProfile()));
      return contactImpl;
    }
    
    if (contactImpl) {
      contactImpl->setRequestingProfile(true);
    }
  } // lock scope
  
  ProfileServiceImpl* pProfileService = ProfileServiceImpl::getInstance();
  SG_ASSERT(pProfileService);
  RequestIdType requestId = pProfileService->createRequestId();
  GetFlag getFlag = Auto;
  if (contactImpl || mode == PSF_ASYNC_FETCH) {
    getFlag = GetFlag(getFlag | AsyncMode);
  }
  ProfilePointer profile = pProfileService->getProfile(requestId, boost::bind(&ProfileSimpleFetcher::callbackProfile, this, _1, _2),
      accountId, getFlag, Level2, ThumbnailPath, false, false);
  if (profile->isDataReturned()) {
    contactImpl = updateContact(profile);
  }

  return contactImpl;
}

bool ProfileSimpleFetcher::modifyContact(const contacts::Contact& c, uint64_t server_time_in_ms)
{
  SGLOG_TRACE("ProfileSimpleFetcher::" << __FUNCTION__ << ", accountId=" << c.getAccountId());

  if(modifyContactImpl(c, server_time_in_ms)){
    notifyCallbacks(c.getAccountId());
  }

  return true;
}

bool ProfileSimpleFetcher::modifyContactImpl(const contacts::Contact& c, int64_t server_time_in_ms)
{
  SGLOG_TRACE("ProfileSimpleFetcher::" << __FUNCTION__ << ", accountId=" << c.getAccountId() << ", timeStamp=" << server_time_in_ms << ", thumbnail_url=" << c.getThumbnailUrl());

  bool bChanged = false;
  contacts::ContactImplPointer contactImpl;

  sgiggle::pr::scoped_lock lock(m_mutex);

  PSFContactMap::iterator it = m_contact_map.find(c.getAccountId());
  if (it == m_contact_map.end()) {
    SGLOG_TRACE("ProfileSimpleFetcher::" << __FUNCTION__ << ", not found");
    return false;
  }

  if (!it->second) {
    SGLOG_TRACE("ProfileSimpleFetcher::" << __FUNCTION__ << ", deleted.");
    return false;
  }
  
  contactImpl = it->second;
  if (contactImpl->getLastModifiedTime() > server_time_in_ms) {
    SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", skip, localModifyTime=" << contactImpl->getLastModifiedTime());

    if (contactImpl->getThumbnailUrlDirect() == contacts::ContactImpl::INVALID_THUMBNAIL_URL &&
        c.getImpl()->getThumbnailUrlDirect() != contacts::ContactImpl::INVALID_THUMBNAIL_URL) {
      SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", set thumbnail url=" << c.getImpl()->getThumbnailUrlDirect());
      contactImpl->setThumbnailUrl(c.getImpl()->getThumbnailUrlDirect());
      contactImpl->setThumbnailPath("");
      bChanged = true;
    }
  } else {
    std::string accountId = c.getAccountId();
    std::string firstName = c.getFirstName();
    std::string lastName = c.getLastName();
    if (lastName.empty() && contacts::ContactManager::isTangoMember(firstName)) {
      firstName = "";
    }

    if (!firstName.empty() || !lastName.empty()) {
      if (contactImpl->getFirstName() != firstName) {
        SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", change first name=" << firstName);
        contactImpl->setFirstName(firstName);
        contactImpl->setDisplayName(""); // to recalculate display name in ContactImpl::getDisplayName()
        bChanged = true;
      }
      if (contactImpl->getLastName() != lastName) {
        SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", change last name=" << lastName);
        contactImpl->setLastName(lastName);
        contactImpl->setDisplayName("");
        bChanged = true;
      }
    }
    if (c.getImpl()->getThumbnailUrlDirect() != contacts::ContactImpl::INVALID_THUMBNAIL_URL &&
        contactImpl->getThumbnailUrl() != c.getImpl()->getThumbnailUrlDirect()) {
      SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", thumbnail url=" << c.getImpl()->getThumbnailUrlDirect());
      contactImpl->setThumbnailUrl(c.getImpl()->getThumbnailUrlDirect());
      contactImpl->setThumbnailPath("");
      bChanged = true;
    }
    if (contactImpl->getLastModifiedTime() != server_time_in_ms) {
      contactImpl->setLastModifiedTime(server_time_in_ms);
      SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", set modifiedTime =" << server_time_in_ms << ", " << contactImpl->getLastModifiedTime());
      bChanged = true;
    }
  }
  m_bCacheTangoFriendsChanged |= bChanged;
  return bChanged;
}

void ProfileSimpleFetcher::deleteContact(const std::string& accountId)
{
  SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", accountId=" << accountId);

  sgiggle::pr::scoped_lock lock(m_mutex);

  m_contact_map[accountId] = contacts::ContactImplPointer();
  m_bCacheTangoFriendsChanged = true;
}

std::string ProfileSimpleFetcher::getThumbnailPath(const std::string& accountId, const std::string& url)
{
  SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", accountId=" << accountId << ", url=" << url);

  ProfileServiceImpl* pProfileService = ProfileServiceImpl::getInstance();
  SG_ASSERT(pProfileService);
  RequestIdType requestId = pProfileService->createRequestId();
  ProfileImagePointer image = pProfileService->getProfileImage(requestId, boost::bind(&ProfileSimpleFetcher::callbackProfileImage, this, _1, _2),
      url, Auto, accountId);

  if (image->isDataReturned()) {
    SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", returned localPath:" << image->localPath() << " for accountId:" << image->accountId());
    return image->localPath();
  }

  return "";
}

void ProfileSimpleFetcher::callbackProfile(boost::shared_ptr<const AsyncTask>, boost::shared_ptr<SocialCallBackDataType> data)
{
  if (!s_bRunning.get())
    return;

  ProfilePointer profile = boost::dynamic_pointer_cast<Profile>(data);
  if (!profile)
  {
    SG_ASSERT(false);
    return;
  }

  if (!profile->isDataReturned())
    return;

  updateContact(profile);
}

void ProfileSimpleFetcher::callbackProfileImage(boost::shared_ptr<const AsyncTask>, boost::shared_ptr<SocialCallBackDataType> data)
{
  SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", requestId=" << data->requestId());

  if (!s_bRunning.get())
    return;

  ProfileImagePointer profileImage = boost::dynamic_pointer_cast<ProfileImage>(data);
  SG_ASSERT(profileImage);

  if (profileImage->errorCode() != SocialCallBackDataType::Success) {
    SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", profileImage error code=" << profileImage->errorCode());
    return;
  }

  contacts::ContactImplPointer contactImpl;
  {
    sgiggle::pr::scoped_lock lock(m_mutex);

    SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", accountId=" << profileImage->accountId() << ", localPath=" << profileImage->localPath());

    PSFContactMap::iterator it = m_contact_map.find(profileImage->accountId());
    if (it == m_contact_map.end()) {
      SGLOG_TRACE("ProfileSimpleFetcher::" << __FUNCTION__ << ", not found");
      return;
    }
    
    if (!it->second) {
      SGLOG_TRACE("ProfileSimpleFetcher::" << __FUNCTION__ << ", deleted.");
      return;
    }
    
    if (it->second->getThumbnailPathDirect() == profileImage->localPath()) {
      SGLOG_TRACE("ProfileSimpleFetcher::" << __FUNCTION__ << ", unchanged.");
      return;
    }
    
    contactImpl = it->second;
    contactImpl->setThumbnailPath(profileImage->localPath());
    m_bCacheTangoFriendsChanged = true;
  } // lock scope
  
  // update ContactManager
  contacts::Contact c(contactImpl);
  contacts::ContactManager::getInstance()->adaptSocialContact(c);
  // others
  notifyCallbacks(profileImage->accountId());
}

contacts::ContactImplPointer ProfileSimpleFetcher::updateContact(ProfilePointer profile)
{
  SGLOG_TRACE("ProfileSimpleFetcher::" << __FUNCTION__);

  const std::string& accountId = profile->userId();
  contacts::ContactImplPointer contactImpl;
  bool bChanged  = false;
  {
    sgiggle::pr::scoped_lock lock(m_mutex);

    if (profile->errorCode() == SocialCallBackDataType::UserNotFound) {
      SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", accountId=" << accountId << ", user not found");
      m_contact_map[accountId] = contacts::ContactImplPointer();
      m_bCacheTangoFriendsChanged = true;
      return contactImpl;
    }

    PSFContactMap::iterator it = m_contact_map.find(accountId);
    SG_ASSERT(it != m_contact_map.end());
    
    if(!it->second) {
      SGLOG_TRACE("ProfileSimpleFetcher::" << __FUNCTION__ << ", deleted.");
      return it->second;
    }
    
    it->second->setRequestingProfile(false);
    if (profile->errorCode() != SocialCallBackDataType::Success) {
      SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", fetch failed. errorCode=" << profile->errorCode());
      return it->second;
    }
    
    contacts::ContactImplPointer temp = contacts::ContactImplPointer(new contacts::ContactImpl);
    temp->setAccountId(accountId);
    temp->setFirstName(profile->firstName());
    temp->setLastName(profile->lastName());
    temp->setThumbnailUrl(profile->thumbnailUrl());
    contacts::Contact c(temp);
    
    if(modifyContactImpl(c, profile->serverModifyTime()))) {
      bChanged = true;
      m_bCacheTangoFriendsChanged = true;
    }
    
    contactImpl = m_contact_map[accountId];
  }

  if(bChanged) {
    contacts::Contact c(contactImpl);
    // notify contact-manager
    contacts::ContactManager::getInstance()->adaptSocialContact(c);
    // others
    notifyCallbacks(accountId);
  }

  return contactImpl;
}

void ProfileSimpleFetcher::notifyCallbacks(const std::string& accountId)
{
  SGLOG_DEBUG("ProfileSimpleFetcher::" << __FUNCTION__ << ", accountId=" << accountId);
  PSFCallbackMap callback_map;
  {
    sgiggle::pr::scoped_lock lock(m_mutex);
    callback_map = m_callback_map;
  }

  BOOST_FOREACH(PSFCallbackMap::value_type& entry, callback_map)
  {
    entry.second(accountId);
  }
}

void ProfileSimpleFetcher::loadFromCache()
{
  std::string buffer;
  sgiggle::local_storage::local_app_data_file::pointer local_data_file =
    sgiggle::local_storage::local_app_data_file::create_in_root_dir(STORAGE_FILE_NAME);
  local_data_file->load(buffer);
  if (buffer.empty())
    return;

  PSFContactMap contact_map;

  sgiggle::xmpp::CachedTangoFriendList friend_list;
  if (friend_list.ParseFromString(buffer))
  {
    for (int i = 0; i < friend_list.contacts_size(); ++i)
    {
      const sgiggle::xmpp::CachedTangoFriend& contact = friend_list.contacts(i);
      contacts::ContactImplPointer contactImpl;
      if (contact.status() == 0)
      {
        contactImpl = contacts::ContactImplPointer(new contacts::ContactImpl);
        contactImpl->setAccountId(contact.accountid());
        contactImpl->setHash(contacts::ContactManager::composeHashByAccountIdForSocialOnly(contact.accountid()));
        contactImpl->setFirstName(contact.firstname());
        contactImpl->setLastName(contact.lastname());
        contactImpl->setThumbnailUrl(contact.thumbnailurl());
        if (contact.has_phonenumber())
        {
          contactImpl->addPhoneNumber(sgiggle::contacts::PhoneNumber(contact.phonenumber().countrycode().countrycodenumber(),
                  contact.phonenumber().subscribernumber(), sgiggle::session::protobufToNativePhoneType(contact.phonenumber().type()) ));
        }
        if (contact.has_email())
        {
          contactImpl->addEmailAddress(contact.email());
        }
        if (contact.has_lastmodifiedtime())
          contactImpl->setLastModifiedTime(contact.lastmodifiedtime());
      }
      contact_map[contact.accountid()] = contactImpl;
    }
  }
  {
    sgiggle::pr::scoped_lock lock(m_mutex);
    m_contact_map.swap(contact_map);
  }
}

void ProfileSimpleFetcher::saveToCache()
{
  POST_IMPl_THIS_CUR_FUNC_IN_THREAD(GET_LOW_PRIORITY_THREAD, saveToCache);

  std::string buffer;

  sgiggle::xmpp::CachedTangoFriendList contacts;
  PSFContactMap contact_map;
  {
    sgiggle::pr::scoped_lock lock(m_mutex);
    if (!m_bCacheTangoFriendsChanged)
      return;
    m_bCacheTangoFriendsChanged = false;
    contact_map = m_contact_map;
  }

  BOOST_FOREACH(const PSFContactMap::value_type& entry, contact_map)
  {
    sgiggle::xmpp::CachedTangoFriend* addedContact = contacts.mutable_contacts()->Add();
    addedContact->set_accountid(entry.first);
    if (entry.second)
    {
      contacts::ContactImplPointer contactImpl = entry.second;
      addedContact->set_status(0);
      addedContact->set_firstname(contactImpl->getFirstName());
      addedContact->set_lastname(contactImpl->getLastName());
      addedContact->set_thumbnailurl(contactImpl->getThumbnailUrl());
      if (contactImpl->getPhoneNumberSize() > 0)
      {
        sgiggle::corefacade::contacts::PhoneNumberPointer pnp = contactImpl->getDefaultPhoneNumber();
        bool bMatched;
        addedContact->mutable_phonenumber()->set_subscribernumber(sgiggle::phone_formatter::format(pnp->subscriberNumber,
            pnp->countryCode, &bMatched));
        addedContact->mutable_phonenumber()->mutable_countrycode()->set_countrycodenumber(pnp->countryCode);
        addedContact->mutable_phonenumber()->set_type(session::nativeToProtobufPhoneType(pnp->phoneType));
      }
      if (contactImpl->getEmailSize() > 0)
        addedContact->set_email(contactImpl->getDefaultEmail());
      addedContact->set_lastmodifiedtime(contactImpl->getLastModifiedTime());
    }
    else
      addedContact->set_status(1);
  }

  contacts.SerializeToString(&buffer);

  sgiggle::local_storage::local_app_data_file::pointer local_data_file =
    sgiggle::local_storage::local_app_data_file::create_in_root_dir(STORAGE_FILE_NAME);
  local_data_file->save(buffer);
}

}}
