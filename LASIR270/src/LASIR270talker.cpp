#include "ros/ros.h"
#include "std_msgs/String.h"
#include <errno.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <cmath>
#include <sstream>
#define TERMINAL    "/dev/ttyUSB0"
#define PI 3.14159265359
int set_interface_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void set_mincount(int fd, int mcount)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error tcgetattr: %s\n", strerror(errno));
        return;
    }

    tty.c_cc[VMIN] = mcount ? 1 : 0;
    tty.c_cc[VTIME] = 5;        /* half second timer */

    if (tcsetattr(fd, TCSANOW, &tty) < 0)
        printf("Error tcsetattr: %s\n", strerror(errno));
}
/**
 * This tutorial demonstrates simple sending of messages over the ROS system.
 */
int main(int argc, char **argv)
{

  char *portname = TERMINAL;
  int fd;
  int wlen;
  char *xstr = "Hello!\n";
  int xlen = strlen(xstr);

  fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 0) {
      printf("Error opening %s: %s\n", portname, strerror(errno));
      return -1;
  }
  /*baudrate 115200, 8 bits, no parity, 1 stop bit */
  set_interface_attribs(fd, B921600);
  //set_mincount(fd, 0);                /* set to pure timed read */

  /* simple output */
  wlen = write(fd, xstr, xlen);
  if (wlen != xlen) {
      printf("Error from write: %d, %d\n", wlen, errno);
  }
  tcdrain(fd);    /* delay for output */


  /**
   * The ros::init() function needs to see argc and argv so that it can perform
   * any ROS arguments and name remapping that were provided at the command line.
   * For programmatic remappings you can use a different version of init() which takes
   * remappings directly, but for most command-line programs, passing argc and argv is
   * the easiest way to do it.  The third argument to init() is the name of the node.
   *
   * You must call one of the versions of ros::init() before using any other
   * part of the ROS system.
   */
  ros::init(argc, argv, "talker");

  /**
   * NodeHandle is the main access point to communications with the ROS system.
   * The first NodeHandle constructed will fully initialize this node, and the last
   * NodeHandle destructed will close down the node.
   */
  ros::NodeHandle n;

  /**
   * The advertise() function is how you tell ROS that you want to
   * publish on a given topic name. This invokes a call to the ROS
   * master node, which keeps a registry of who is publishing and who
   * is subscribing. After this advertise() call is made, the master
   * node will notify anyone who is trying to subscribe to this topic name,
   * and they will in turn negotiate a peer-to-peer connection with this
   * node.  advertise() returns a Publisher object which allows you to
   * publish messages on that topic through a call to publish().  Once
   * all copies of the returned Publisher object are destroyed, the topic
   * will be automatically unadvertised.
   *
   * The second parameter to advertise() is the size of the message queue
   * used for publishing messages.  If messages are published more quickly
   * than we can send them, the number here specifies how many messages to
   * buffer up before throwing some away.
   */
  ros::Publisher chatter_pub = n.advertise<std_msgs::String>("chatter", 1000);

  ros::Rate loop_rate(20);

  /**
   * A count of how many messages we have sent. This is used to create
   * a unique string for each message.
   */
  float distance;
  float motor_angle;
  float x,y,z;  
  while (ros::ok())
  {

#pragma region start of data read
       unsigned char buf[2];
        int rdlen;
        rdlen = read(fd, buf, sizeof(buf) - 1);
        if (rdlen > 0) 
        {
#ifdef DISPLAY_STRING
            buf[rdlen] = 0;
            printf("Read %d: \"%s\"\n", rdlen, buf);
#else 
            unsigned char   *p;
            printf("Read %d:", rdlen);
            for (p = buf; rdlen-- > 0; p++)
            {
                printf(" 0x%x", *p);
                if( *p == 170 )
                {
                    //read 5 times more
                    unsigned char total_data[5];
                    for (size_t i = 0; i < 5; i++)
                    {
                       unsigned char buffer[2];
                       rdlen = read(fd, buffer, sizeof(buffer) - 1);
                       total_data[i] = buffer[0];
                       buffer[rdlen] = 0;
                       if(rdlen > 0 )
                       {
                           unsigned char* data = buffer; 
                           printf(" data 0x%x", *data);
                       }
                    }
                    // convert data and send
                    distance =  ( total_data[2] + total_data[3] << 8 ) ;
                    motor_angle =  ( total_data[0] << 8 ) + total_data[1];
                    printf(" /n motor angle  == %d  motor distance == %d  " , motor_angle , distance); 

                    // !!!!!!!! distance will be calibrated
                    float motor_angle_to_radian =  motor_angle/500.0f * PI; 
                    x = distance * cos(motor_angle_to_radian);
                    y = distance * sin(motor_angle_to_radian);
                    z = 0.0f;
                }
            }
            printf("\n");
#endif
        } 
        else if (rdlen < 0) 
        {
            printf("Error from read: %d: %s\n", rdlen, strerror(errno));
        } 
        else 
        {  
            printf("Timeout from read\n");
        }
#pragma endregion end of data read

    /**
     * This is a message object. You stuff it with data, and then publish it.
     */
    std_msgs::String msg;

    std::stringstream ss;
    std::string pub_data = "";
    pub_data += std::to_string(x);
    pub_data += " ";
    pub_data += std::to_string(y);
    pub_data += " ";
    pub_data += std::to_string(z);
    pub_data += " ";
    msg.data = pub_data;

    ROS_INFO("%s", msg.data.c_str());

    /**
     * The publish() function is how you send messages. The parameter
     * is the message object. The type of this object must agree with the type
     * given as a template parameter to the advertise<>() call, as was done
     * in the constructor above.
     */
    chatter_pub.publish(msg);

    ros::spinOnce();

    loop_rate.sleep();
  }


  return 0;
}

