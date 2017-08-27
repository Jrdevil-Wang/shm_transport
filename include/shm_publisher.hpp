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

#define RETRY 2
    // allocation shm message
    uint32_t serlen = ros::serialization::serializationLength(msg);
    ShmMessage * ptr = NULL;
    // bad_alloc exception may occur if some ros messages are lost
    int attempt = 0;
    for (; attempt < RETRY && ptr == NULL; attempt++) {
      try {
        ptr = (ShmMessage *)pobj_->pshm_->allocate(sizeof(ShmMessage) + serlen);
      } catch (boost::interprocess::bad_alloc e) {
        pobj_->plck_->lock();
        // ROS_INFO("bad_alloc happened, releasing the oldest and trying again...");
        ShmMessage * first_msg =
          (ShmMessage *)pobj_->pshm_->get_address_from_handle(pobj_->pmsg_->getFirstHandle());
        if (first_msg->ref != 0) {
          pobj_->plck_->unlock();
          ROS_WARN("the oldest is in use, abandon this message <%p>...", &msg);
          break;
        }
        // free the oldest message, and try again
        pobj_->pmsg_->releaseFirst(pobj_->pshm_);
        pobj_->plck_->unlock();
      }
    }
    if (ptr) {
      // construct shm message
      pobj_->plck_->lock();
      ptr->construct(pobj_);
      pobj_->plck_->unlock();
      // serialize data
      ptr->len = serlen;
      ros::serialization::OStream out(ptr->data, serlen);
      ros::serialization::serialize(out, msg);
      // publish the real message (handle of ShmStruct)
      std_msgs::UInt64 actual_msg;
      actual_msg.data = pobj_->pshm_->get_handle_from_address(ptr);
      pub_.publish(actual_msg);
    } else if (attempt >= RETRY) {
      ROS_WARN("bad_alloc happened %d times, abandon this message <%p>...", attempt, &msg);
    } else {

    }
#undef RETRY
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
    pobj_ = ShmObjectPtr(new ShmObject(pshm, t));
  }

  ros::Publisher pub_;
  ShmObjectPtr   pobj_;

friend class Topic;
};

} // namespace shm_transport

#endif // __SHM_PUBLISHER_HPP__

