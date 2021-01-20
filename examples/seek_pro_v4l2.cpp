/*
 *  Test program SEEK Thermal CompactPRO
 *  Author: Maarten Vandersteegen
 */
#include <opencv2/highgui/highgui.hpp>
#include "seek.h"
#include <iostream>

// used to send the data to v4l2
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    LibSeek::SeekThermalPro seek(argc == 2 ? argv[1] : "");
    cv::Mat frame, grey_frame;

    if (!seek.open()) {
        std::cout << "failed to open seek cam" << std::endl;
        return -1;
    }


    int width = 320;
    int height = 240;
    int vidsendsiz = 0;

    int v4l2lo = open("/dev/video1", O_WRONLY); 
    if(v4l2lo < 0) {
        std::cout << "Error opening v4l2l device: " << strerror(errno);
        exit(-2);
    }
    {
        struct v4l2_format v;
        int t;
        v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        t = ioctl(v4l2lo, VIDIOC_G_FMT, &v);
        if( t < 0 ) {
            exit(t);
        }
        v.fmt.pix.width = width;
        v.fmt.pix.height = height;
        v.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
        vidsendsiz = width * height * 3;
        v.fmt.pix.sizeimage = vidsendsiz;
        t = ioctl(v4l2lo, VIDIOC_S_FMT, &v);
        if( t < 0 ) {
            exit(t);
        }
    }

    while(1)
    {
        if (!seek.read(frame)) {
            std::cout << "no more LWIR img" << std::endl;
            return -1;
        }

        cv::Mat imgStream(frame.rows,  frame.cols, CV_8UC3, cv::Scalar(0, 255, 0));
        for(int y=0;y<imgStream.rows;y++)
        {
            for(int x=0;x<imgStream.cols;x++)
            {
                cv::Vec3b color = imgStream.at<cv::Vec3b>(cv::Point(x,y));
                int16_t value = frame.at<int16_t>(cv::Point(x,y));
                color.val[0] = 0;
                color.val[1] = (value & 0xFF00) >> 8;
                color.val[2] = value & 0x00FF;
                imgStream.at<cv::Vec3b>(cv::Point(x,y)) = color;
            }
        }

        int size = imgStream.total() * imgStream.elemSize();
        size_t written = write(v4l2lo, imgStream.data, size);
        if (written < 0) {
            std::cout << "Error writing v4l2l device";
            close(v4l2lo);
            return 1;
        }

        // cv::imshow("LWIR", grey_frame);

        // char c = cv::waitKey(10);
        // if (c == 's') {
        //     cv::waitKey(0);
        // }
    }
}
