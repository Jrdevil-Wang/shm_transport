#ifndef __SHM_SUBSCRIBER_HPP__
#define __SHM_SUBSCRIBER_HPP__

#include <boost/interprocess/managed_shared_memory.hpp>
#include "ros/ros.h"
#include "std_msgs/UInt64.h"
#include "shm_object.hpp"

namespace shm_transport
{

class Topic;

template <class M>
class Subscriber;

template < class M >
class SubscriberCallbackHelper
{
  typedef void (*Func)(const boost::shared_ptr< const M > &);

public:
  ~SubscriberCallbackHelper() {
  }

  void callback(const std_msgs::UInt64::ConstPtr & actual_msg) {
    if (!pobj_) {
      // If suscriber runs first, it will not die due to these code.
      mng_shm * pshm = new mng_shm(boost::interprocess::open_only, name_.c_str());
      pobj_ = ShmObjectPtr(new ShmObject(pshm, name_));
    }
    // get shm message
    ShmMessage * ptr = (ShmMessage *)pobj_->pshm_->get_address_from_handle(actual_msg->data);
    // take shm message
    ptr->take();
    // deserialize data
    boost::shared_ptr< M > msg(new M());
    ros::serialization::IStream in(ptr->data, ptr->len);
    ros::serialization::deserialize(in, *msg);
    // release shm message
    ptr->release();
    // call user callback
    fp_(msg);
  }

private:
  SubscriberCallbackHelper(const std::string &topic, Func fp)
    : pobj_(), name_(topic), fp_(fp) {
    // change '/' in topic to '_'
    for (int i = 0; i < name_.length(); i++)
      if (name_[i] == '/')
        name_[i] = '_';
  }

  ShmObjectPtr pobj_;
  std::string name_;
  Func fp_;

friend class Topic;
friend class Subscriber<M>;
};

template <class M>
class Subscriber
{
public:
  Subscriber() {
  }

  ~Subscriber() {
  }

  Subscriber(const Subscriber & s) {
    *this = s;
  }

  Subscriber & operator = (const Subscriber & s) {
    sub_ = s.sub_;
    phlp_ = s.phlp_;
    return *this;
  }

  void shutdown() {
    sub_.shutdown();
  }

  std::string getTopic() const {
    return sub_.getTopic();
  }

  uint32_t getNumPublishers() const {
    return sub_.getNumPublishers();
  }

private:
  Subscriber(const ros::Subscriber & sub, SubscriberCallbackHelper< M > * phlp)
      : sub_(sub), phlp_(phlp) {
  }

  ros::Subscriber sub_;
  boost::shared_ptr< SubscriberCallbackHelper< M > > phlp_;

friend class Topic;
};

} // namespace shm_transport

#endif // __SHM_SUBSCRIBER_HPP__

