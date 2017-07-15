#ifndef __SHM_SUBSCRIBER_HPP__
#define __SHM_SUBSCRIBER_HPP__

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/atomic/atomic.hpp>
#include "ros/ros.h"
#include "std_msgs/UInt64.h"

namespace shm_transport {

  class Topic;

  template <class M>
  class Subscriber;

  template < class M >
  class SubscriberCallbackHelper {
    typedef void (*Func)(const boost::shared_ptr< const M > &);

    private:
      SubscriberCallbackHelper(const std::string &topic, Func fp)
        : pshm_(NULL), topic_(topic), fp_(fp), sub_((ros::Subscriber *)NULL) {
      }

    public:
      ~SubscriberCallbackHelper() {
        if (pshm_) {
          boost::atomic<uint32_t> *ref_ptr = pshm_->find_or_construct<boost::atomic<uint32_t> >("ref")(0);
          if (ref_ptr->fetch_sub(1, boost::memory_order_relaxed) == 1) {
            boost::interprocess::shared_memory_object::remove(sub_->getTopic().c_str());
            ROS_INFO("shm file <%s> removed\n", sub_->getTopic().c_str());
          }
          delete pshm_;
        }
      }

      void callback(const std_msgs::UInt64::ConstPtr & actual_msg) {
        if (!pshm_) {
          pshm_ = new boost::interprocess::managed_shared_memory(boost::interprocess::open_only, topic_.c_str());
          boost::atomic<uint32_t> *ref_ptr = pshm_->find_or_construct<boost::atomic<uint32_t> >("ref")(0);
          ref_ptr->fetch_add(1, boost::memory_order_relaxed);
        }
        // FIXME this segment should be locked
        uint32_t * ptr = (uint32_t *)pshm_->get_address_from_handle(actual_msg->data);
        M msg;
        ros::serialization::IStream in((uint8_t *)(ptr + 2), ptr[1]);
        ros::serialization::deserialize(in, msg);
        // FIXME is boost::atomic rely on x86?
        if (reinterpret_cast< boost::atomic< uint32_t > * >(ptr)->fetch_sub(1, boost::memory_order_relaxed) == 1) {
          pshm_->deallocate(ptr);
        }
        fp_(boost::make_shared< M >(msg));
      }

    private:
      boost::interprocess::managed_shared_memory * pshm_;
      std::string topic_;
      Func fp_;
      boost::shared_ptr< ros::Subscriber > sub_;

    friend class Topic;
    friend class Subscriber<M>;
  };

  template <class M>
  class Subscriber {
    public:
      Subscriber(const Subscriber & s) {
        sub_ = s.sub_;
        phlp_ = s.phlp_;
      }
    private:
      Subscriber(const ros::Subscriber & sub, SubscriberCallbackHelper< M > * phlp) {
        sub_ = boost::make_shared< ros::Subscriber >(sub);
        phlp_ = phlp;
        phlp_->sub_ = sub_;
      }

    public:
      ~Subscriber() {
        if (phlp_)
          delete phlp_;
        if (sub_)
          shutdown();
      }

      void shutdown() {
        sub_->shutdown();
      }

      std::string getTopic() const {
        return sub_->getTopic();
      }

      uint32_t getNumPublishers() const {
        return sub_->getNumPublishers();
      }

    protected:
      boost::shared_ptr< ros::Subscriber > sub_;
      SubscriberCallbackHelper< M > * phlp_;

    friend class Topic;
  };

}

#endif // __SHM_SUBSCRIBER_HPP__

