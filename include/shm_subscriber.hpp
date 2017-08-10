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
              if(sub_) {
                  boost::interprocess::shared_memory_object::remove(sub_->getTopic().c_str());
                  //printf("subscriber: shm file <%s> removed\n", sub_->getTopic().c_str());
                }
            }
          delete pshm_;
        }
    }

    void callback(const std_msgs::UInt64::ConstPtr & actual_msg) {
      if (!pshm_) {   //a small trick. If suscriber runs first, it will not die due to these code.
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
  private:
    Subscriber(const ros::Subscriber & sub_, SubscriberCallbackHelper< M > * phlp) {
      impl_ = boost::make_shared<Impl>();

      impl_->sub = boost::make_shared< ros::Subscriber >(sub_);
      impl_->phlp = phlp;
      impl_->phlp->sub_ = boost::make_shared< ros::Subscriber >(sub_);
    }

    class Impl {
    public:
      Impl() {
      }

      ~Impl() {
        if (phlp) {
            delete phlp;
          }
        if (sub) {
            sub->shutdown();
          }
      }

      boost::shared_ptr< ros::Subscriber > sub;
      SubscriberCallbackHelper< M > * phlp;
    };

    boost::shared_ptr<Impl> impl_;

  public:
    Subscriber() {
    }

    ~Subscriber() {
    }

    Subscriber(const Subscriber & s) {
      impl_ = s.impl_;
    }

    void shutdown() {
      impl_->sub->shutdown();
    }

    std::string getTopic() const {
      return impl_->sub->getTopic();
    }

    uint32_t getNumPublishers() const {
      return impl_->sub->getNumPublishers();
    }

  protected:

    friend class Topic;
  };

}

#endif // __SHM_SUBSCRIBER_HPP__

