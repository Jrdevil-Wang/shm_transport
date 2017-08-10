#ifndef __SHM_PUBLISHER_HPP__
#define __SHM_PUBLISHER_HPP__

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/atomic/atomic.hpp>
#include "ros/ros.h"

namespace shm_transport {

  class Topic;

  class Publisher {
  private:
    Publisher(const ros::Publisher & pub_, const std::string & topic, uint32_t mem_size) {
      impl_ = boost::make_shared<Impl>();
      impl_->pub = boost::make_shared< ros::Publisher >(pub_);
      impl_->pshm = new boost::interprocess::managed_shared_memory(boost::interprocess::open_or_create, topic.c_str(), mem_size);
      boost::atomic<uint32_t> *ref_ptr = impl_->pshm->find_or_construct<boost::atomic<uint32_t> >("ref")(0);
      ref_ptr->fetch_add(1, boost::memory_order_relaxed);
    }

    class Impl {
    public:
      Impl() {
      }

      ~Impl() {
        if (pshm) {
            boost::atomic<uint32_t> * ref_ptr = pshm->find_or_construct<boost::atomic<uint32_t> >("ref")(0);
            if (ref_ptr->fetch_sub(1, boost::memory_order_relaxed) == 1) {
                if(pub)
                  {
                    boost::interprocess::shared_memory_object::remove(pub->getTopic().c_str());
                    //printf("publisher: shm file <%s>　removed\n", pub->getTopic().c_str());
                  }
              }
            delete pshm;
          }
        if (pub) {
            pub->shutdown();
          }
      }

      boost::shared_ptr< ros::Publisher > pub;
      boost::interprocess::managed_shared_memory * pshm;
    };

    boost::shared_ptr<Impl> impl_;

  public:
    Publisher() {
    }

    Publisher(const Publisher& rhs) {
      impl_ = rhs.impl_;
    }

    ~Publisher() {

    }

    template < class M >
    void publish(const M & msg) const {
      if (!impl_->pshm)
        return;
      if (impl_->pub->getNumSubscribers() == 0)
        return;

      uint32_t serlen = ros::serialization::serializationLength(msg);  //BUGFIX TODO when a subscriber exit, there may be one msg left in shm
      uint32_t * ptr = (uint32_t *)impl_->pshm->allocate(sizeof(uint32_t) * 2 + serlen);
      ptr[0] = impl_->pub->getNumSubscribers();
      ptr[1] = serlen;
      ros::serialization::OStream out((uint8_t *)(ptr + 2), serlen);
      ros::serialization::serialize(out, msg);

      std_msgs::UInt64 actual_msg;
      actual_msg.data = impl_->pshm->get_handle_from_address(ptr);
      impl_->pub->publish(actual_msg);
    }

    void shutdown() {
      impl_->pub->shutdown();
    }

    std::string getTopic() const {
      return impl_->pub->getTopic();
    }

    uint32_t getNumSubscribers() const {
      return impl_->pub->getNumSubscribers();
    }

  protected:

    friend class Topic;
  };
}

#endif // __SHM_PUBLISHER_HPP__

