#ifndef __SHM_OBJECT_HPP__
#define __SHM_OBJECT_HPP__

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/atomic/atomic.hpp>

namespace shm_transport
{

// some typedef for short
typedef boost::atomic< uint32_t > atomic_uint32_t;
typedef boost::interprocess::managed_shared_memory mng_shm;
typedef boost::shared_ptr< boost::interprocess::managed_shared_memory > mng_shm_ptr;
typedef boost::interprocess::interprocess_mutex ipc_mutex;

class MsgListHead
{
public:
  MsgListHead() : next(0), prev(0) { }
  ~MsgListHead() { }

  void addLast(MsgListHead * lc, const mng_shm_ptr & pshm) {
    long hc = pshm->get_handle_from_address(lc);
    long hn = pshm->get_handle_from_address(this), hp = this->prev;
    MsgListHead * ln = this, * lp = (MsgListHead *)pshm->get_address_from_handle(hp);
    lc->next = hn;
    lc->prev = hp;
    lp->next = hc;
    ln->prev = hc;
  }

  void remove(MsgListHead * lc, const mng_shm_ptr & pshm) {
    long hc = pshm->get_handle_from_address(lc);
    long hn = lc->next, hp = lc->prev;
    MsgListHead * ln = (MsgListHead *)pshm->get_address_from_handle(hn);
    MsgListHead * lp = (MsgListHead *)pshm->get_address_from_handle(hp);
    lp->next = hn;
    ln->prev = hp;
  }

  void releaseFirst(const mng_shm_ptr & pshm) {
    long hc = next;
    MsgListHead * lc = (MsgListHead *)pshm->get_address_from_handle(hc);
    if (lc == this)
      return;
    long hn = lc->next, hp = lc->prev;
    MsgListHead * ln = (MsgListHead *)pshm->get_address_from_handle(hn), * lp = this;
    lp->next = hn;
    ln->prev = hp;
    pshm->deallocate(lc);
  }

public:
  long next;
  long prev;
};

class ShmObject
{
public:
  ShmObject(mng_shm * pshm, std::string name, bool sub)
      : pshm_(pshm), name_(name), sub_(sub) {
    pref_ = pshm_->find_or_construct< atomic_uint32_t >("ref")(0);
    psub_ = pshm_->find_or_construct< uint32_t >("sub")(0);
    plck_ = pshm_->find_or_construct< ipc_mutex >("lck")();
    pmsg_ = pshm_->find_or_construct< MsgListHead >("lst")();

    pref_->fetch_add(1, boost::memory_order_relaxed);
    if (sub_) {
      plck_->lock();
      (*psub_)++;
      plck_->unlock();
    }
    if (pmsg_->next == 0) {
      long handle = pshm_->get_handle_from_address(pmsg_);
      pmsg_->next = handle;
      pmsg_->prev = handle;
    }
  }

  ~ShmObject() {
    if (pref_->fetch_sub(1, boost::memory_order_relaxed) == 1) {
      boost::interprocess::shared_memory_object::remove(name_.c_str());
      //printf("shm file <%s> removed\n", name_.c_str());
    } else if (sub_) {
      plck_->lock();
      (*psub_)--;
      plck_->unlock();
    }
  }

public:
  // smart pointer of shm
  mng_shm_ptr pshm_;
  // name of shm
  std::string name_;
  // whether this is a subscriber
  bool sub_;
  // in shm, reference count (pub # + sub #)
  atomic_uint32_t * pref_;
  // in shm, subscription count (sub #)
  uint32_t * psub_;
  // in shm, connection lock
  ipc_mutex * plck_;
  // in shm, head node of double-linked message list
  MsgListHead * pmsg_;
};
typedef boost::shared_ptr< ShmObject > ShmObjectPtr;

class ShmMessage
{
public:
  void construct(const ShmObjectPtr & so) {
    // insert into message list
    so->pmsg_->addLast(&lst, so->pshm_);
    // set reference count and data length
    ref = *(so->psub_);
  }

  void destruct(const ShmObjectPtr & so) {
    if (ref.fetch_sub(1, boost::memory_order_relaxed) == 1) {
      // remove from message list
      so->pmsg_->remove(&lst, so->pshm_);
      // deallocate message
      so->pshm_->deallocate(this);
    }
  }

public:
  MsgListHead		lst;
  atomic_uint32_t	ref;
  uint32_t			len;
  uint8_t			data[0];
};

} // namespace shm_transport

#endif // __SHM_PUBLISHER_HPP__

