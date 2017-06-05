#include "bpvo/vo.h"
#include "bpvo/trajectory.h"
#include "bpvo/utils.h"
#include "bpvo/timer.h"
#include "utils/program_options.h"
#include "utils/viz.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include<opencv2/opencv.hpp>

//
// vo example, with trajectory written to file
//

static const char* LEFT_IMAGE_PREFIX =
//"/home/halismai/data/NewTsukubaStereoDataset/illumination/fluorescent/left/tsukuba_fluorescent_L_%05d.png";
"/home/sourav/workspace/NTSD-200/fluorescent/left/frame_%d.png";

static const char* DMAP_PREFIX =
//"/home/halismai/data/NewTsukubaStereoDataset/groundtruth/disparity_maps/left/tsukuba_disparity_L_%05d.png";
"/home/sourav/workspace/NTSD-200/disparity_maps/left/frame_%d.png";

using namespace bpvo;

void colorizeDisparity(const cv::Mat& src, cv::Mat& dst, double min_d, double num_d)
{
  THROW_ERROR_IF( src.type() != cv::DataType<float>::type, "disparity must be float" );

  double scale = 0.0;
  if(num_d > 0) {
    scale = 255.0 / num_d;
  } else {
    double max_val = 0;
    cv::minMaxLoc(src, nullptr, &max_val);
    scale = 255.0 / max_val;
  }

  src.convertTo(dst, CV_8U, scale);
  cv::applyColorMap(dst, dst, cv::COLORMAP_JET);

  for(int y = 0; y < src.rows; ++y)
    for(int x = 0; x < src.cols; ++x)
      if(src.at<float>(y,x) <= min_d)
        dst.at<cv::Vec3b>(y,x) = cv::Vec3b(0,0,0);
}

void overlayDisparity2(const cv::Mat& I, const cv::Mat& D, cv::Mat& dst,
                      double alpha, double min_d, double num_d)
{
  cv::Mat image;
  switch( I.type() )
  {
    case CV_8UC1:
      cv::cvtColor(I, image, CV_GRAY2BGR);
      break;
    case CV_8UC3:
      image = I;
      break;
    case CV_8UC4:
      cv::cvtColor(I, image, CV_BGRA2BGR);
      break;
    default:
      THROW_ERROR("unsupported image type");
  }

  cv::addWeighted(image, alpha, bpvo::colorizeDisparity(D, min_d, num_d), 1.0-alpha, 0.0, dst);
}

static AlgorithmParameters GetAlgorithmParameters()
{
  AlgorithmParameters params;
  params.numPyramidLevels = 4;
  params.maxIterations = 100;
  params.parameterTolerance = 1e-6;
  params.functionTolerance = 1e-6;
  params.verbosity = VerbosityType::kSilent;
  params.minTranslationMagToKeyFrame = 0.1;
  params.minRotationMagToKeyFrame = 2.5;
  params.maxFractionOfGoodPointsToKeyFrame = 0.7;
  params.goodPointThreshold = 0.8;

  return params;
}

struct ImageAndDisparity
{
  ImageAndDisparity() {}

  inline bool empty() const { return image.empty() || disparity.empty(); }

  cv::Mat image, disparity;
}; // ImageAndDisparity

bool GetImageAndDisparity(ImageAndDisparity& ret, int f_i)
{
  ret.image = cv::imread(Format(LEFT_IMAGE_PREFIX, f_i), cv::IMREAD_GRAYSCALE);
  ret.disparity = cv::imread(Format(DMAP_PREFIX, f_i), cv::IMREAD_GRAYSCALE);

  if(ret.empty()) {
    Warn("Failed to read frame number %d\n", f_i);
    return false;
  } else {
    ret.disparity.convertTo(ret.disparity, CV_32FC1);
    return true;
  }
}

/**
 * Example of using VO and storing the output to a text file.
 *
 * We only store the trajectory (in the KITTI format) as well as the (X,Y,Z)
 * camera path.
 *
 * To store the point cloud as well as other statistics see apps/vo_app.cc
 */
int main(int argc, char** argv)
{
  Matrix33 K; K << 615.0, 0.0, 320.0, 0.0, 615.0, 240.0, 0.0, 0.0, 1.0;
  float b = 0.1;
  VisualOdometry vo(K, b, ImageSize(480, 640), GetAlgorithmParameters());
  Trajectory trajectory;

  auto nframes = 200;// options.get<int>("numframes");
  double total_time = 0.0f;

  ImageAndDisparity frame;
  for(int i = 1; i < nframes; ++i) {
    if(!GetImageAndDisparity(frame, i))
      break;

    Timer timer;
    auto result = vo.addFrame(frame.image.ptr<uint8_t>(), frame.disparity.ptr<float>());
    auto tt = timer.stop().count();
    total_time += ( tt / 1000.0f);

    fprintf(stdout, "Frame %03d [%03d ms] %0.2f Hz\r", i, (int) tt, i / total_time);
    fflush(stdout);

    trajectory.push_back( result.pose );
    std::cout << result.pose << std::endl;

    cv::Mat dmap = frame.disparity.clone();
//    colorizeDisparity(dmap,dmap,1.0,128.0);
    overlayDisparity2(frame.image,dmap,dmap,0.5,1.0,128.0);
    cv::imshow("dmap",dmap);
    cv::waitKey(1);
  }

  printf("\nProcessed %d frames @ %0.2f Hz\n", nframes, nframes / total_time);

//  auto output_fn = options.get<std::string>("output");
//  if(!output_fn.empty()) {
//    trajectory.write(output_fn + ".txt"); // store all the poses as 4x4 matrices
//    // write the camera center patch (x,y,z) only wrt to the first added frame
//    trajectory.writeCameraPath(output_fn + "_path.txt");
//  }

  return 0;
}


