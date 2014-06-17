// (c) 2004-2012 Coverity, Inc. All rights reserved worldwide.
#ifndef __EMPTY_STATE_H__
#define __EMPTY_STATE_H__

#include "state.hpp"
#include "caching/empty-cache.hpp"

class empty_state_t : public state_t
{
public:
  
  // construct a state_t with an arena
  empty_state_t(mc_arena a, sm_t *sm) : state_t(a, sm) { }

  // Create an empty cache
  cache_t *create_empty_cache(mc_arena a) const;
  
  virtual state_t* clone(mc_arena a) const;

  void write_as_text(StateWriter &out) const;

  override bool is_empty_state() const;
};

#endif // __EMPTY_STATE_H__
