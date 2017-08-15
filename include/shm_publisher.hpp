#ifndef __SHM_PUBLISHER_HPP__
#define __SHM_PUBLISHER_HPP__

#include <boost/interprocess/managed_shared_memory.hpp>
#include "ros/ros.h"
#include "std_msgs/UInt64.h"
#include "shm_object.hpp"

namespace shm_transport
{

class Topic;

class Publisher
{
public:
  Publisher() {
  }

  ~Publisher() {
  }

  Publisher(const Publisher & p) {
    *this = p;
  }

  Publisher & operator = (const Publisher & p) {
    pub_ = p.pub_;
    pobj_ = p.pobj_;
    return *this;
  }

  template < class M >
  void publish(const M & msg) const {
    if (!pobj_)
      return;

    pobj_->plck_->lock();
    if (*(pobj_->psub_) > 0) {
#define RETRY 2
      // allocation shm message
      uint32_t serlen = ros::serialization::serializationLength(msg);
      ShmMessage * ptr = NULL;
      // bad_alloc exception may occur if some ros messages are lost
      for (int i = 0; i < RETRY && ptr == NULL; i++) {
        try {
          ptr = (ShmMessage *)pobj_->pshm_->allocate(sizeof(ShmMessage) + serlen);
        } catch (boost::interprocess::bad_alloc e) {
          // free the oldest message, and try again
          pobj_->pmsg_->releaseFirst(pobj_->pshm_);
        }
      }
      if (ptr) {
        // construct shm message
        ptr->construct(pobj_);
        // serialize data
        ptr->len = serlen;
        ros::serialization::OStream out(ptr->data, serlen);
        ros::serialization::serialize(out, msg);
        // publish the real message (handle of ShmStruct)
        std_msgs::UInt64 actual_msg;
        actual_msg.data = pobj_->pshm_->get_handle_from_address(ptr);
        pub_.publish(actual_msg);
      } else {
        ROS_INFO("bad_alloc happen %d times, abandon this message <%p>...", RETRY, &msg);
      }
#undef RETRY
    } else {
      // publish the dummy message, notify any new subscribers
      std_msgs::UInt64 actual_msg;
      actual_msg.data = 0;
      pub_.publish(actual_msg);
    }
    pobj_->plck_->unlock();
  }

  void shutdown() {
    pub_.shutdown();
  }

  std::string getTopic() const {
    return pub_.getTopic();
  }

  uint32_t getNumSubscribers() const {
    return pub_.getNumSubscribers();
  }

private:
  Publisher(const ros::Publisher & pub, const std::string & topic, uint32_t mem_size)
      : pub_(pub) {
    // change '/' in topic to '_'
    std::string t = topic;
    for (int i = 0; i < t.length(); i++)
      if (t[i] == '/')
        t[i] = '_';
    mng_shm * pshm = new mng_shm(boost::interprocess::open_or_create, t.c_str(), mem_size);
    pobj_ = ShmObjectPtr(new ShmObject(pshm, t, false));
  }

  ros::Publisher pub_;
  ShmObjectPtr   pobj_;

friend class Topic;
};

} // namespace shm_transport

#endif // __SHM_PUBLISHER_HPP__

