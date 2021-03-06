/*
 * Software License Agreement (Modified BSD License)
 *
 *  Copyright (c) 2016, PAL Robotics, S.L.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of PAL Robotics, S.L. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * @author Sammy Pfeiffer
 */

#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <camera_info_manager/camera_info_manager.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <cv_bridge/cv_bridge.h>
#include <sstream>
#include <boost/assign/list_of.hpp>
#include <string>
#include <iostream>

// Based on the ros tutorial on transforming opencv images to Image messages

sensor_msgs::CameraInfo get_default_camera_info_from_image(sensor_msgs::ImagePtr img){
    sensor_msgs::CameraInfo cam_info_msg;
    cam_info_msg.header.frame_id = img->header.frame_id;
    // Fill image size
    cam_info_msg.height = img->height;
    cam_info_msg.width = img->width;
    ROS_INFO_STREAM("The image width is: " << img->width);
    ROS_INFO_STREAM("The image height is: " << img->height);
    // Add the most common distortion model as sensor_msgs/CameraInfo says
    cam_info_msg.distortion_model = "plumb_bob";
    // Don't let distorsion matrix be empty
    cam_info_msg.D.resize(5, 0.0);
    // Give a reasonable default intrinsic camera matrix
    cam_info_msg.K = boost::assign::list_of(1.0) (0.0) (img->width/2.0)
                                           (0.0) (1.0) (img->height/2.0)
                                           (0.0) (0.0) (1.0);
    // Give a reasonable default rectification matrix
    cam_info_msg.R = boost::assign::list_of (1.0) (0.0) (0.0)
                                            (0.0) (1.0) (0.0)
                                            (0.0) (0.0) (1.0);
    // Give a reasonable default projection matrix
    cam_info_msg.P = boost::assign::list_of (1.0) (0.0) (img->width/2.0) (0.0)
                                            (0.0) (1.0) (img->height/2.0) (0.0)
                                            (0.0) (0.0) (1.0) (0.0);
    return cam_info_msg;
}


std::string getImgType(int imgTypeInt)
{
    int numImgTypes = 35; // 7 base types, with five channel options each (none or C1, ..., C4)

    int enum_ints[] =       {CV_8U,  CV_8UC1,  CV_8UC2,  CV_8UC3,  CV_8UC4,
                             CV_8S,  CV_8SC1,  CV_8SC2,  CV_8SC3,  CV_8SC4,
                             CV_16U, CV_16UC1, CV_16UC2, CV_16UC3, CV_16UC4,
                             CV_16S, CV_16SC1, CV_16SC2, CV_16SC3, CV_16SC4,
                             CV_32S, CV_32SC1, CV_32SC2, CV_32SC3, CV_32SC4,
                             CV_32F, CV_32FC1, CV_32FC2, CV_32FC3, CV_32FC4,
                             CV_64F, CV_64FC1, CV_64FC2, CV_64FC3, CV_64FC4};

    std::string enum_strings[] = {"CV_8U",  "CV_8UC1",  "CV_8UC2",  "CV_8UC3",  "CV_8UC4",
                             "CV_8S",  "CV_8SC1",  "CV_8SC2",  "CV_8SC3",  "CV_8SC4",
                             "CV_16U", "CV_16UC1", "CV_16UC2", "CV_16UC3", "CV_16UC4",
                             "CV_16S", "CV_16SC1", "CV_16SC2", "CV_16SC3", "CV_16SC4",
                             "CV_32S", "CV_32SC1", "CV_32SC2", "CV_32SC3", "CV_32SC4",
                             "CV_32F", "CV_32FC1", "CV_32FC2", "CV_32FC3", "CV_32FC4",
                             "CV_64F", "CV_64FC1", "CV_64FC2", "CV_64FC3", "CV_64FC4"};

    for(int i=0; i<numImgTypes; i++)
    {
        if(imgTypeInt == enum_ints[i]) return enum_strings[i];
    }
    return "unknown image type";
}

int main(int argc, char** argv)
{
    ros::init(argc, argv, "image_publisher");
    ros::NodeHandle nh;
    ros::NodeHandle _nh("~"); // to get the private params
    image_transport::ImageTransport it(nh);
    image_transport::CameraPublisher pub = it.advertiseCamera("camera", 1);

    // provider can be an url (e.g.: rtsp://10.0.0.1:554) or a number of device, (e.g.: 0 would be /dev/video0)
    std::string video_stream_provider;
    cv::VideoCapture cap;
    if (_nh.getParam("video_stream_provider", video_stream_provider)){
        ROS_INFO_STREAM("Resource video_stream_provider: " << video_stream_provider);
        // If we are given a string of 4 chars or less (I don't think we'll have more than 100 video devices connected)
        // treat is as a number and act accordingly so we open up the videoNUMBER device
        if (video_stream_provider.size() < 4){
            ROS_INFO_STREAM("Getting video from provider: /dev/video" << video_stream_provider);
            cap.open(atoi(video_stream_provider.c_str()));
        }
        else{
            ROS_INFO_STREAM("Getting video from provider: " << video_stream_provider);
            cap.open(video_stream_provider);
        }
    }
    else{
        ROS_ERROR("Failed to get param 'video_stream_provider'");
        return -1;
    }


    std::string camera_name;
    _nh.param("camera_name", camera_name, std::string("camera"));
    ROS_INFO_STREAM("Camera name: " << camera_name);

    int fps;
    _nh.param("fps", fps, 240);
    ROS_INFO_STREAM("Throttling to fps: " << fps);

    std::string frame_id;
    _nh.param("frame_id", frame_id, std::string("camera"));
    ROS_INFO_STREAM("Publishing with frame_id: " << frame_id);

    std::string camera_info_url;
    _nh.param("camera_info_url", camera_info_url, std::string(""));
    ROS_INFO_STREAM("Provided camera_info_url: '" << camera_info_url << "'");

    bool flip_horizontal;
    _nh.param("flip_horizontal", flip_horizontal, false);
    ROS_INFO_STREAM("Flip horizontal image is: " << ((flip_horizontal)?"true":"false"));

    bool flip_vertical;
    _nh.param("flip_vertical", flip_vertical, false);
    ROS_INFO_STREAM("Flip vertical image is: " << ((flip_vertical)?"true":"false"));
 
    //sensor_msg/encoding type  
    std::string msg_encoding;
    _nh.param("msg_encoding", msg_encoding, std::string("bgr8"));
    ROS_INFO_STREAM("Ros message encoding: " << msg_encoding);

  
    int width_target;
    int height_target;
    _nh.param("width", width_target, 0);
    _nh.param("height", height_target, 0);
    if (width_target != 0 && height_target != 0){
        ROS_INFO_STREAM("Forced image width is: " << width_target);
        ROS_INFO_STREAM("Forced image height is: " << height_target);
    }

    // From http://docs.opencv.org/modules/core/doc/operations_on_arrays.html#void flip(InputArray src, OutputArray dst, int flipCode)
    // FLIP_HORIZONTAL == 1, FLIP_VERTICAL == 0 or FLIP_BOTH == -1
    bool flip_image = true;
    int flip_value;
    if (flip_horizontal && flip_vertical)
        flip_value = 0; // flip both, horizontal and vertical
    else if (flip_horizontal)
        flip_value = 1;
    else if (flip_vertical)
        flip_value = -1;
    else
        flip_image = false;

    if(!cap.isOpened()){
        ROS_ERROR_STREAM("Could not open the stream.");
        return -1;
    }
 
    //TODO: move try block to here
   
    try
    {  
      //set width/height
      if (width_target != 0 && height_target != 0){
        cap.set(CV_CAP_PROP_FRAME_WIDTH, width_target);
        cap.set(CV_CAP_PROP_FRAME_HEIGHT, height_target);
      }
      // set to monochrome
      //cap.set(CV_CAP_PROP_MONOCHROME,1);
      // Boolean flags indicating whether images should be converted to RGB.
      cap.set(CV_CAP_PROP_CONVERT_RGB, 0);
      cap.set(CAP_OPENNI_IMAGE_GENERATOR_OUTPUT_MODE, CAP_OPENNI_VGA_30HZ );

      //display native image format
      ROS_INFO_STREAM("Raw video format: " << cap.get(CV_CAP_PROP_FORMAT) );
      ROS_INFO_STREAM("Video format: " << getImgType(cap.get(CV_CAP_PROP_FORMAT)) );
    
      //set image format
      //TODO: deletecap.set(CV_CAP_PROP_FORMAT, CV_16UC1); //msg_encoding); //TODO; convert string to const

      //display new  image format
      //TOOD: delete ROS_INFO_STREAM("Raw video format: " << cap.get(CV_CAP_PROP_FORMAT) );

      //TODO: CV_CAP_PROP_MODE is specific to kinect to being set to CV_CAP_MODE_YUYV
      //cap.set(CV_CAP_PROP_MODE, CV_CAP_MODE_YUYV); 
      //cap.set(CV_CAP_PROP_MODE, CV_CAP_MODE_GRAY);
          //https://stackoverflow.com/questions/27496698/opencv-capture-yuyv-from-camera-without-rgb-conversion 
          //https://docs.opencv.org/3.3.0/d4/d15/group__videoio__flags__base.html#gad0f42b32af0d89d2cee80dae0ea62b3d
          //http://docs.ros.org/kinetic/api/sensor_msgs/html/image__encodings_8h_source.html

    }
    //catch (cv_bridge::Exception &e)
    catch (ros::Exception &e)
    {
      ROS_INFO_STREAM("Failed  message: " << e.what());
      ROS_ERROR("Failed  message: %s", e.what());
      return(1);
    }

    ROS_INFO_STREAM("Opened the stream, starting to publish.");

    cv::Mat frame;
    cv::Mat frame_16UC3(width_target,height_target,CV_16UC3);
    cv::Mat frame_gray16UC1(width_target,height_target,CV_16UC1);
    sensor_msgs::ImagePtr msg;
    sensor_msgs::CameraInfo cam_info_msg;
    std_msgs::Header header;
    header.frame_id = frame_id;
    camera_info_manager::CameraInfoManager cam_info_manager(nh, camera_name, camera_info_url);
    // Get the saved camera info if any
    cam_info_msg = cam_info_manager.getCameraInfo();

    //TODO: lets grab one image from the stream to get the encoding type
    cap>>frame;
        if( !frame.empty() ) 
        {
            std::string imgFmt=getImgType(frame.type());
            ROS_INFO_STREAM("Raw Video Stream Image type: " << imgFmt );
            //TODO: set to grescale
            try
            {  
                //TODO: if type =8U*, then
                  //change 8 bit depth to 16
                  //https://github.com/ros-perception/vision_opencv/blob/kinetic/cv_bridge/src/cv_bridge.cpp#L331
                  //http://docs.ros.org/kinetic/api/sensor_msgs/html/image__encodings_8h_source.html
                  frame.convertTo(frame_16UC3,CV_16UC3,65535. / 255.);
                  imgFmt=getImgType(frame_16UC3.type());
                  ROS_INFO_STREAM("frame_16UC3 Video Stream Image type: " << imgFmt );
                  //Convert to single channel
                  cv::cvtColor(frame_16UC3, frame_gray16UC1, CV_BGR2GRAY ); //note opencv3 uses cv::COLOR_BGR2GRAY
                  imgFmt=getImgType(frame_gray16UC1.type());
                  ROS_INFO_STREAM("frame_gray16UC1 Video Stream Image type: " << imgFmt );
                
            }
            catch (ros::Exception &e)
            {
              //ROS_INFO_STREAM("Failed  message: " << e.what());
              //ROS_ERROR("Failed  message: %s", e.what());
              std::cout << "Error: " << e.what() << std::endl;
              ros::spinOnce(); 
              //return(1);
           }
      
            
        }    

    ros::Rate r(fps);
    while (nh.ok()) { // TODO: should also have cap.isOpened? 
        cap >> frame; //same as cap.read(frame)
                     // which does cap.grab , then cap.retrieve and 
        if (pub.getNumSubscribers() > 0){
            // Check if grabbed frame is actually full with some content
            if(!frame.empty()) {
                // Flip the image if necessary
                if (flip_image)
                    cv::flip(frame, frame, flip_value);


                // convert 8UC3 to needed  depthimage grescale input format 16UC1
                //TODO: if type =8U*, then
                try
                {  
                  //change 8 bit depth to 16
                  //https://github.com/ros-perception/vision_opencv/blob/kinetic/cv_bridge/src/cv_bridge.cpp#L331
                  //http://docs.ros.org/kinetic/api/sensor_msgs/html/image__encodings_8h_source.html
                  frame.convertTo(frame_16UC3,CV_16UC3,65535. / 255.);
                  
                  //Now Convert to single channel
                  cv::cvtColor(frame_16UC3, frame_gray16UC1, CV_BGR2GRAY ); //note opencv3 uses cv::COLOR_BGR2GRAY
                  //msg = cv_bridge::CvImage(header, msg_encoding, frame).toImageMsg();
                  msg = cv_bridge::CvImage(header, msg_encoding, frame_gray16UC1).toImageMsg();
                
                }
                catch (ros::Exception &e)
                {
                  ROS_INFO_STREAM("Failed  message: " << e.what());
                  //ROS_ERROR("Failed  message: %s", e.what());
                  std::cout << "Error: " << e.what() << std::endl;
                  ros::spinOnce(); 
                  return(1);
                }

                // Create a default camera info if we didn't get a stored one on initialization
                if (cam_info_msg.distortion_model == ""){
                    ROS_WARN_STREAM("No calibration file given, publishing a reasonable default camera info.");
                    cam_info_msg = get_default_camera_info_from_image(msg);
                    cam_info_manager.setCameraInfo(cam_info_msg);
                }
                // The timestamps are in sync thanks to this publisher
                pub.publish(*msg, cam_info_msg, ros::Time::now());
            }

            ros::spinOnce();
        }
        r.sleep();
    }
}
