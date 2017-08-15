#include "ros/ros.h"
#include "std_msgs/String.h"
#include <sstream>
#include "shm_topic.hpp"

#define MSGLEN (1920 * 1080 * 3)
#define HZ (30)

std::string str(MSGLEN, '-');
char tmp[100];

int main(int argc, char ** argv) {
  ros::init(argc, argv, "shm_talker", ros::init_options::AnonymousName);
  ros::NodeHandle n;
  shm_transport::Topic t(n);
  shm_transport::Publisher p = t.advertise< std_msgs::String >("shm_test_topic", HZ, 3 * MSGLEN);

  ros::Rate loop_rate(HZ);
  int count = 0;
  while (ros::ok()) {
    int len = snprintf(tmp, 100, "message # %d ...", count);
    memcpy(&str[0], tmp, len);

    std_msgs::String msg;
    msg.data = str;

    ROS_INFO("info: [%s]", tmp);
    p.publish(msg);

    ros::spinOnce();
    loop_rate.sleep();
    count++;
  }
  return 0;
}

