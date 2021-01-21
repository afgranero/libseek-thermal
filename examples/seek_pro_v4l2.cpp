/*
 *  Test program SEEK Thermal CompactPRO
 *  Author: Maarten Vandersteegen
 */
#include <opencv2/highgui/highgui.hpp>
#include "seek.h"
#include <iostream>
#include "args.h"


#include <string>
#include <memory>
#include <sstream>


// used to send the data to v4l2
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>

int setup_v4l2(std::string output, int width,int height)
{
    int vidsendsiz = 0;

    int v4l2lo = open(output.c_str(), O_WRONLY); 
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
    return v4l2lo;
}

void v4l2_out(int v4l2lo, cv::Mat& imgStream)
{
    int size = imgStream.total() * imgStream.elemSize();
    int written = write(v4l2lo, imgStream.data, size);
    if (written < 0) {
        std::cout << "Error writing v4l2l device";
        close(v4l2lo);
        exit(1);
    }
}

cv::Mat process_frame(cv::Mat frame)
{
    cv::Mat imgStream(frame.rows,  frame.cols, CV_8UC3, cv::Scalar(0, 255, 0));
    for(int y=0;y<imgStream.rows;y++)
    {
        for(int x=0;x<imgStream.cols;x++)
        {
            cv::Vec3b color = imgStream.at<cv::Vec3b>(cv::Point(x,y));
            int16_t value = frame.at<int16_t>(cv::Point(x,y));
            color.val[0] = 0;
            color.val[1] = (value & 0xFF00) >> 8; // encode MSB of raw value on G channel
            color.val[2] = value & 0x00FF;        // encode LSB of raw value on B channel
            imgStream.at<cv::Vec3b>(cv::Point(x,y)) = color;
        }
    }
    return imgStream;
}

int main(int argc, char** argv)
{
    // args::ArgumentParser parser("Seek Compactpro v4l2 driver");
    args::ArgumentParser parser("Seek Thermal Viewer");
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    args::ValueFlag<std::string> _ffc(parser, "FFC", "Additional Flat Field calibration - provide ffc file", {'F', "FFC"});
    args::ValueFlag<std::string> _output(parser, "output", "Name of the file or video device to write to", {'o', "output"});

    // Parse arguments
    try {
        parser.ParseCLI(argc, argv);
    }
    catch (args::Help) 
    {
        std::cout << parser;
        return 0;
    }
    catch (args::ParseError e) 
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }
    catch (args::ValidationError e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    std::string output = "";
    if (_output)
        output = args::get(_output);

    if (output.empty())
    {
        std::cout << "Specify a video device to output to eg: /dev/video1" << std::endl;
        return 1;
    }


    LibSeek::SeekThermalPro seek(args::get(_ffc), 500, 10);
    cv::Mat frame, grey_frame;

    if (!seek.open()) {
        std::cout << "Failed to open seek cam" << std::endl;
        return -1;
    }

    int width = 320;
    int height = 240;
    int v4l2lo = setup_v4l2(output, width, height);

    while(1)
    {
        if (!seek.read(frame)) {
            std::cout << "No more LWIR img" << std::endl;
            return -1;
        }

        
        cv::Mat imgStream = process_frame(frame);

        v4l2_out(v4l2lo, imgStream);


        // cv::imshow("LWIR", grey_frame);

        // char c = cv::waitKey(10);
        // if (c == 's') {
        //     cv::waitKey(0);
        // }
    }
}
