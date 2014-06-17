/**
 * \file sink-deriver-state.cpp
 *
 * (c) 2012-2013 Coverity, Inc. All rights reserved worldwide.
**/
#include "sink-deriver-state.hpp"

#include "analysis/iface-tracker/iface-tracker.hpp" // sink_iface_info_t, etc.
#include "analysis/traversal/abstract-interp.hpp" // abstract_interp_t::get_arena()
#include "analysis/ast/ast/cc.ast.hpp" // S_return

sink_deriver_state_t::
sink_deriver_state_t(arena_t *a, sm_t *sm) :
    ignored_ifaces_set_t(
        a,
        sm,
        /*continue_if_new_object*/false,
        "Ignored interfaces") {
}

ignored_ifaces_set_t::set_type &sink_deriver_state_t::
ignored_ifaces() {
    return s;
}

const ignored_ifaces_set_t::set_type &sink_deriver_state_t::
ignored_ifaces() const {
    return s;
}

model_t* sink_deriver_state_t::
create_model_get_info(
    const Expression *t,
    sink_iface_info_t &info,
    abstract_interp_t &cur_traversal) const
{
    if(!find_iface_info(t, info, cur_traversal)) {
        return NULL;
    }
    model_t *rv = get_derive_sm()->create_model(
        cur_traversal.get_arena(),
        *info.iface,
        cur_traversal);
    rv->add_events(info.alias_events);
    return rv;
}

model_t* sink_deriver_state_t::
create_model(const Expression *t,
             abstract_interp_t &cur_traversal) const
{
    sink_iface_info_t info;
    return create_model_get_info(t, info, cur_traversal);
}

// Indicates that the given interface has been explicitly ignored.
bool sink_deriver_state_t::
is_ignored(const action_interface_t *iface) const
{
    if(::contains(ignored_ifaces(), iface)) {
        return true;
    }
    // If the iface is p->field, see if *p is ignored.
    if(const field_interface_t *f =
       iface->iffield_interface_tC()) {
        // Use arena to prevent freeing
        deref_interface_t d(NULL, f->base);
        bool rv = ::contains(ignored_ifaces(), &d);
        // Prevent freeing base
        d.base = NULL;
        return rv;
    }
    return false;
}

// Indicates that the given expression is an sink interface that
// hasn't been ignored.
bool sink_deriver_state_t::
is_iface(const Expression *expr,
         abstract_interp_t &cur_traversal) const
{
    sink_iface_info_t info;
    return find_iface_info(expr, info, cur_traversal);
}

// If the given expression is an sink interface that
// hasn't been ignored, return that interface.
bool sink_deriver_state_t::
find_iface_info(
    const Expression *expr,
    sink_iface_info_t &info,
    abstract_interp_t &cur_traversal) const
{
    if(!get_derive_sm()->find_sink_iface_info(
           expr,
           info,
           cur_traversal)) {
        return false;
    }

    return !is_ignored(info.iface);
}

const action_interface_t *sink_deriver_state_t::
find_iface(
    const Expression *expr,
    abstract_interp_t &cur_traversal,
    const event_list_t *&alias_events) const
{
    sink_iface_info_t info;

    if(!find_iface_info(
            expr,
            info,
            cur_traversal)) {
        return NULL;
    }
    alias_events = info.alias_events;
    return info.iface;
}

void sink_deriver_state_t::
ignore(const Expression *t, abstract_interp_t &cur_traversal)
{
    const event_list_t *alias_events;
    const action_interface_t *iface =
        get_derive_sm()->find_sink_iface(
            t, cur_traversal, alias_events);
    if(iface) {
        ignore(iface);
    }
}

void sink_deriver_state_t::
ignore(const action_interface_t *iface)
{
    ignored_ifaces().insert(iface);
}

void sink_deriver_state_t::
commit_model(model_t *m, abstract_interp_t &cur_traversal)
{
    get_derive_sm()->commit_model(m, cur_traversal);
    ignore(&m->get_iface());
}

sink_deriver_state_t* sink_deriver_state_t::clone(arena_t *a) const
{
    // We don't have any data, so do the same thing as the
    // set_state_t.
    // We have to override "clone" so that the dynamic type is correct
    // though.
    sink_deriver_state_t *rv =
        new (a) sink_deriver_state_t(a, get_derive_sm());
    rv->copyFrom(a, *this);
    return rv;
}

void addSimpleReturnEvent(
    model_t *model,
    const S_return *ret,
    abstract_interp_t &cur_traversal,
    void *arg)
{
    model->add_event(
        "return",
        desc() << "Returning " << *ret->expr << ".",
        ret->expr);
}
