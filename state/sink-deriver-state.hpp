/**
 * \file sink-deriver-state.hpp
 *
 * State useful to write a "sink" deriver.
 * The "sink_deriver_state_t" only keeps track of a set of ignored
 * interfaces (whose purpose is e.g. to disable a sink if some check
 * happened on the interface), and includes convenience function to
 * create sink models.
 *
 * (c) 2012 Coverity, Inc. All rights reserved worldwide.
**/
#ifndef SINK_DERIVER_STATE_HPP
#define SINK_DERIVER_STATE_HPP

#include "set-state.hpp"                        // set_state_t
#include "libs/parse-errors/model-typedefs.hpp" // action_interface_t

#include "analysis/sm/derive-sm.hpp"            // derive_sm_t

// fwds for those are enough for this header, but clients will very
// likely need them anyway.
#include "analysis/sm/model.hpp"                // model_t

// A store of ignored interfaces.
typedef set_state_t<const action_interface_t *, action_interface_t::ptr_lt_t>
ignored_ifaces_set_t;


class sink_deriver_state_t :
    public ignored_ifaces_set_t
{
public:

    // The 'sm' argument should be a derive_sm_t, but it has to be declared
    // just sm_t for compatibility with the state composition machinery.
    sink_deriver_state_t(arena_t *a, sm_t *sm);

    ignored_ifaces_set_t::set_type &ignored_ifaces();
    const ignored_ifaces_set_t::set_type &ignored_ifaces() const;

    /**
     * Create a model for the interface represented by the given
     * expression, on "cur_traversal.get_arena()" (it's meant to be
     * committed immediately)
     * Returns NULL if the expression doesn't represent a sink
     * interface, or if that interface is ignored.
     **/
    model_t* create_model(
        const Expression *t,
        abstract_interp_t &cur_traversal
    ) const;

    /**
     * Same as above, but also obtains the sink_iface_info_t (See that
     * struct in iface-tracker.hpp)
     **/
    model_t* create_model_get_info(
        const Expression *t,
        sink_iface_info_t &info,
        abstract_interp_t &cur_traversal
    ) const;

    // Indicates that the given interface has been explicitly ignored.
    bool is_ignored(const action_interface_t *iface) const;

    bool is_ignored(const action_interface_t &iface) const
    {
        return is_ignored(&iface);
    }

    /**
     * Indicates that the given expression is an sink interface that
     * hasn't been ignored.
     **/
    bool is_iface(
        const Expression *expr,
        abstract_interp_t &cur_traversal) const;

    /**
     * If the given expression is an sink interface that
     * hasn't been ignored, return that interface.
     **/
    const action_interface_t *find_iface(
        const Expression *expr,
        abstract_interp_t &cur_traversal,
        const event_list_t *&alias_events) const;

    /**
     * Same as above, but obtains the sink_iface_info_t (See that
     * struct in iface-tracker.hpp)
     **/
    bool find_iface_info(
        const Expression *expr,
        sink_iface_info_t &info,
        abstract_interp_t &cur_traversal) const;

    /**
     * Indicate that a given expression, and the interface it
     * represents (if any) should be ignored for the rest of
     * this path.
     * Note that the interface tracker already takes care of
     * assignments.
     **/
    void ignore(const Expression *t, abstract_interp_t &cur_traversal);

    /**
     * Ignore the given interface. See above.
     **/
    void ignore(const action_interface_t *iface);

    /**
     * Commits the model, then ignores its interface.
     * In general, once you've committed a sink model for a given
     * iface, you don't want to do it again.
     **/
    void commit_model(model_t *m, abstract_interp_t &cur_traversal);

    override sink_deriver_state_t* clone(arena_t *a) const;

protected:

    derive_sm_t *get_derive_sm() const {
        return static_cast<derive_sm_t *>(this->sm);
    }
};

#endif // SINK_DERIVER_STATE_HPP
