#ifdef USE_OPENCV
#include <opencv2/core/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#endif  // USE_OPENCV

#include <string>
#include <vector>

#include "caffe/data_transformer.hpp"
#include "caffe/util/io.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/util/rng.hpp"

namespace caffe {

template<typename Dtype>
DataTransformer<Dtype>::DataTransformer(const TransformationParameter& param,
    Phase phase)
    : param_(param), phase_(phase) {
  // check if we want to use mean_file
  if (param_.has_mean_file()) {
    CHECK_EQ(param_.mean_value_size(), 0) <<
      "Cannot specify mean_file and mean_value at the same time";
    const string& mean_file = param.mean_file();
    if (Caffe::root_solver()) {
      LOG(INFO) << "Loading mean file from: " << mean_file;
    }
    BlobProto blob_proto;
    ReadProtoFromBinaryFileOrDie(mean_file.c_str(), &blob_proto);
    data_mean_.FromProto(blob_proto);
  }
  // check if we want to use mean_value
  if (param_.mean_value_size() > 0) {
    CHECK(param_.has_mean_file() == false) <<
      "Cannot specify mean_file and mean_value at the same time";
    for (int c = 0; c < param_.mean_value_size(); ++c) {
      mean_values_.push_back(param_.mean_value(c));
    }
  }
  // check if we want to do random scaling
  if (param_.scale_factors_size() > 0) {
    for (int i = 0; i < param_.scale_factors_size(); ++i) {
      scale_factors_.push_back(param_.scale_factors(i));
    }
  }  
}

template<typename Dtype>
void DataTransformer<Dtype>::Transform(const Datum& datum,
                                       Dtype* transformed_data) {
  const string& data = datum.data();
  const int datum_channels = datum.channels();
  const int datum_height = datum.height();
  const int datum_width = datum.width();

  const int crop_size = param_.crop_size();
  const Dtype scale = param_.scale();
  const bool do_mirror = param_.mirror() && Rand(2);
  const bool has_mean_file = param_.has_mean_file();
  const bool has_uint8 = data.size() > 0;
  const bool has_mean_values = mean_values_.size() > 0;

  CHECK_GT(datum_channels, 0);
  CHECK_GE(datum_height, crop_size);
  CHECK_GE(datum_width, crop_size);

  Dtype* mean = NULL;
  if (has_mean_file) {
    CHECK_EQ(datum_channels, data_mean_.channels());
    CHECK_EQ(datum_height, data_mean_.height());
    CHECK_EQ(datum_width, data_mean_.width());
    mean = data_mean_.mutable_cpu_data();
  }
  if (has_mean_values) {
    CHECK(mean_values_.size() == 1 || mean_values_.size() == datum_channels) <<
     "Specify either 1 mean_value or as many as channels: " << datum_channels;
    if (datum_channels > 1 && mean_values_.size() == 1) {
      // Replicate the mean_value for simplicity
      for (int c = 1; c < datum_channels; ++c) {
        mean_values_.push_back(mean_values_[0]);
      }
    }
  }

  int height = datum_height;
  int width = datum_width;

  int h_off = 0;
  int w_off = 0;
  if (crop_size) {
    height = crop_size;
    width = crop_size;
    // We only do random crop when we do training.
    if (phase_ == TRAIN) {
      h_off = Rand(datum_height - crop_size + 1);
      w_off = Rand(datum_width - crop_size + 1);
    } else {
      h_off = (datum_height - crop_size) / 2;
      w_off = (datum_width - crop_size) / 2;
    }
  }

  Dtype datum_element;
  int top_index, data_index;
  for (int c = 0; c < datum_channels; ++c) {
    for (int h = 0; h < height; ++h) {
      for (int w = 0; w < width; ++w) {
        data_index = (c * datum_height + h_off + h) * datum_width + w_off + w;
        if (do_mirror) {
          top_index = (c * height + h) * width + (width - 1 - w);
        } else {
          top_index = (c * height + h) * width + w;
        }
        if (has_uint8) {
          datum_element =
            static_cast<Dtype>(static_cast<uint8_t>(data[data_index]));
        } else {
          datum_element = datum.float_data(data_index);
        }
        if (has_mean_file) {
          transformed_data[top_index] =
            (datum_element - mean[data_index]) * scale;
        } else {
          if (has_mean_values) {
            transformed_data[top_index] =
              (datum_element - mean_values_[c]) * scale;
          } else {
            transformed_data[top_index] = datum_element * scale;
          }
        }
      }
    }
  }
}


template<typename Dtype>
void DataTransformer<Dtype>::Transform(const Datum& datum,
                                       Blob<Dtype>* transformed_blob) {
  // If datum is encoded, decoded and transform the cv::image.
  if (datum.encoded()) {
#ifdef USE_OPENCV
    CHECK(!(param_.force_color() && param_.force_gray()))
        << "cannot set both force_color and force_gray";
    cv::Mat cv_img;
    if (param_.force_color() || param_.force_gray()) {
    // If force_color then decode in color otherwise decode in gray.
      cv_img = DecodeDatumToCVMat(datum, param_.force_color());
    } else {
      cv_img = DecodeDatumToCVMatNative(datum);
    }
    // Transform the cv::image into blob.
    return Transform(cv_img, transformed_blob);
#else
    LOG(FATAL) << "Encoded datum requires OpenCV; compile with USE_OPENCV.";
#endif  // USE_OPENCV
  } else {
    if (param_.force_color() || param_.force_gray()) {
      LOG(ERROR) << "force_color and force_gray only for encoded datum";
    }
  }

  const int crop_size = param_.crop_size();
  const int datum_channels = datum.channels();
  const int datum_height = datum.height();
  const int datum_width = datum.width();

  // Check dimensions.
  const int channels = transformed_blob->channels();
  const int height = transformed_blob->height();
  const int width = transformed_blob->width();
  const int num = transformed_blob->num();

  CHECK_EQ(channels, datum_channels);
  CHECK_LE(height, datum_height);
  CHECK_LE(width, datum_width);
  CHECK_GE(num, 1);

  if (crop_size) {
    CHECK_EQ(crop_size, height);
    CHECK_EQ(crop_size, width);
  } else {
    CHECK_EQ(datum_height, height);
    CHECK_EQ(datum_width, width);
  }

  Dtype* transformed_data = transformed_blob->mutable_cpu_data();
  Transform(datum, transformed_data);
}

template<typename Dtype>
void DataTransformer<Dtype>::Transform(const vector<Datum> & datum_vector,
                                       Blob<Dtype>* transformed_blob) {
  const int datum_num = datum_vector.size();
  const int num = transformed_blob->num();
  const int channels = transformed_blob->channels();
  const int height = transformed_blob->height();
  const int width = transformed_blob->width();

  CHECK_GT(datum_num, 0) << "There is no datum to add";
  CHECK_LE(datum_num, num) <<
    "The size of datum_vector must be no greater than transformed_blob->num()";
  Blob<Dtype> uni_blob(1, channels, height, width);
  for (int item_id = 0; item_id < datum_num; ++item_id) {
    int offset = transformed_blob->offset(item_id);
    uni_blob.set_cpu_data(transformed_blob->mutable_cpu_data() + offset);
    Transform(datum_vector[item_id], &uni_blob);
  }
}

#ifdef USE_OPENCV
template<typename Dtype>
void DataTransformer<Dtype>::Transform(const vector<cv::Mat> & mat_vector,
                                       Blob<Dtype>* transformed_blob) {
  const int mat_num = mat_vector.size();
  const int num = transformed_blob->num();
  const int channels = transformed_blob->channels();
  const int height = transformed_blob->height();
  const int width = transformed_blob->width();

  CHECK_GT(mat_num, 0) << "There is no MAT to add";
  CHECK_EQ(mat_num, num) <<
    "The size of mat_vector must be equals to transformed_blob->num()";
  Blob<Dtype> uni_blob(1, channels, height, width);
  for (int item_id = 0; item_id < mat_num; ++item_id) {
    int offset = transformed_blob->offset(item_id);
    uni_blob.set_cpu_data(transformed_blob->mutable_cpu_data() + offset);
    Transform(mat_vector[item_id], &uni_blob);
  }
}

template<typename Dtype>
void DataTransformer<Dtype>::Transform(const cv::Mat& cv_img,
                                       Blob<Dtype>* transformed_blob) {
  const int crop_size = param_.crop_size();
  const int img_channels = cv_img.channels();
  const int img_height = cv_img.rows;
  const int img_width = cv_img.cols;

  // Check dimensions.
  const int channels = transformed_blob->channels();
  const int height = transformed_blob->height();
  const int width = transformed_blob->width();
  const int num = transformed_blob->num();

  CHECK_EQ(channels, img_channels);
  CHECK_LE(height, img_height);
  CHECK_LE(width, img_width);
  CHECK_GE(num, 1);

  CHECK(cv_img.depth() == CV_8U) << "Image data type must be unsigned byte";

  const Dtype scale = param_.scale();
  const bool do_mirror = param_.mirror() && Rand(2);
  const bool has_mean_file = param_.has_mean_file();
  const bool has_mean_values = mean_values_.size() > 0;

  CHECK_GT(img_channels, 0);
  CHECK_GE(img_height, crop_size);
  CHECK_GE(img_width, crop_size);

  Dtype* mean = NULL;
  if (has_mean_file) {
    CHECK_EQ(img_channels, data_mean_.channels());
    CHECK_EQ(img_height, data_mean_.height());
    CHECK_EQ(img_width, data_mean_.width());
    mean = data_mean_.mutable_cpu_data();
  }
  if (has_mean_values) {
    CHECK(mean_values_.size() == 1 || mean_values_.size() == img_channels) <<
     "Specify either 1 mean_value or as many as channels: " << img_channels;
    if (img_channels > 1 && mean_values_.size() == 1) {
      // Replicate the mean_value for simplicity
      for (int c = 1; c < img_channels; ++c) {
        mean_values_.push_back(mean_values_[0]);
      }
    }
  }

  int h_off = 0;
  int w_off = 0;
  cv::Mat cv_cropped_img = cv_img;
  if (crop_size) {
    CHECK_EQ(crop_size, height);
    CHECK_EQ(crop_size, width);
    // We only do random crop when we do training.
    if (phase_ == TRAIN) {
      h_off = Rand(img_height - crop_size + 1);
      w_off = Rand(img_width - crop_size + 1);
    } else {
      h_off = (img_height - crop_size) / 2;
      w_off = (img_width - crop_size) / 2;
    }
    cv::Rect roi(w_off, h_off, crop_size, crop_size);
    cv_cropped_img = cv_img(roi);
  } else {
    CHECK_EQ(img_height, height);
    CHECK_EQ(img_width, width);
  }

  CHECK(cv_cropped_img.data);

  Dtype* transformed_data = transformed_blob->mutable_cpu_data();
  int top_index;
  for (int h = 0; h < height; ++h) {
    const uchar* ptr = cv_cropped_img.ptr<uchar>(h);
    int img_index = 0;
    for (int w = 0; w < width; ++w) {
      for (int c = 0; c < img_channels; ++c) {
        if (do_mirror) {
          top_index = (c * height + h) * width + (width - 1 - w);
        } else {
          top_index = (c * height + h) * width + w;
        }
        // int top_index = (c * height + h) * width + w;
        Dtype pixel = static_cast<Dtype>(ptr[img_index++]);
        if (has_mean_file) {
          int mean_index = (c * img_height + h_off + h) * img_width + w_off + w;
          transformed_data[top_index] =
            (pixel - mean[mean_index]) * scale;
        } else {
          if (has_mean_values) {
            transformed_data[top_index] =
              (pixel - mean_values_[c]) * scale;
          } else {
            transformed_data[top_index] = pixel * scale;
          }
        }
      }
    }
  }
}
#endif  // USE_OPENCV

template<typename Dtype>
void DataTransformer<Dtype>::Transform(Blob<Dtype>* input_blob,
                                       Blob<Dtype>* transformed_blob) {
  const int crop_size = param_.crop_size();
  const int input_num = input_blob->num();
  const int input_channels = input_blob->channels();
  const int input_height = input_blob->height();
  const int input_width = input_blob->width();

  if (transformed_blob->count() == 0) {
    // Initialize transformed_blob with the right shape.
    if (crop_size) {
      transformed_blob->Reshape(input_num, input_channels,
                                crop_size, crop_size);
    } else {
      transformed_blob->Reshape(input_num, input_channels,
                                input_height, input_width);
    }
  }

  const int num = transformed_blob->num();
  const int channels = transformed_blob->channels();
  const int height = transformed_blob->height();
  const int width = transformed_blob->width();
  const int size = transformed_blob->count();

  CHECK_LE(input_num, num);
  CHECK_EQ(input_channels, channels);
  CHECK_GE(input_height, height);
  CHECK_GE(input_width, width);


  const Dtype scale = param_.scale();
  const bool do_mirror = param_.mirror() && Rand(2);
  const bool has_mean_file = param_.has_mean_file();
  const bool has_mean_values = mean_values_.size() > 0;

  int h_off = 0;
  int w_off = 0;
  if (crop_size) {
    CHECK_EQ(crop_size, height);
    CHECK_EQ(crop_size, width);
    // We only do random crop when we do training.
    if (phase_ == TRAIN) {
      h_off = Rand(input_height - crop_size + 1);
      w_off = Rand(input_width - crop_size + 1);
    } else {
      h_off = (input_height - crop_size) / 2;
      w_off = (input_width - crop_size) / 2;
    }
  } else {
    CHECK_EQ(input_height, height);
    CHECK_EQ(input_width, width);
  }

  Dtype* input_data = input_blob->mutable_cpu_data();
  if (has_mean_file) {
    CHECK_EQ(input_channels, data_mean_.channels());
    CHECK_EQ(input_height, data_mean_.height());
    CHECK_EQ(input_width, data_mean_.width());
    for (int n = 0; n < input_num; ++n) {
      int offset = input_blob->offset(n);
      caffe_sub(data_mean_.count(), input_data + offset,
            data_mean_.cpu_data(), input_data + offset);
    }
  }

  if (has_mean_values) {
    CHECK(mean_values_.size() == 1 || mean_values_.size() == input_channels) <<
     "Specify either 1 mean_value or as many as channels: " << input_channels;
    if (mean_values_.size() == 1) {
      caffe_add_scalar(input_blob->count(), -(mean_values_[0]), input_data);
    } else {
      for (int n = 0; n < input_num; ++n) {
        for (int c = 0; c < input_channels; ++c) {
          int offset = input_blob->offset(n, c);
          caffe_add_scalar(input_height * input_width, -(mean_values_[c]),
            input_data + offset);
        }
      }
    }
  }

  Dtype* transformed_data = transformed_blob->mutable_cpu_data();

  for (int n = 0; n < input_num; ++n) {
    int top_index_n = n * channels;
    int data_index_n = n * channels;
    for (int c = 0; c < channels; ++c) {
      int top_index_c = (top_index_n + c) * height;
      int data_index_c = (data_index_n + c) * input_height + h_off;
      for (int h = 0; h < height; ++h) {
        int top_index_h = (top_index_c + h) * width;
        int data_index_h = (data_index_c + h) * input_width + w_off;
        if (do_mirror) {
          int top_index_w = top_index_h + width - 1;
          for (int w = 0; w < width; ++w) {
            transformed_data[top_index_w-w] = input_data[data_index_h + w];
          }
        } else {
          for (int w = 0; w < width; ++w) {
            transformed_data[top_index_h + w] = input_data[data_index_h + w];
          }
        }
      }
    }
  }
  if (scale != Dtype(1)) {
    DLOG(INFO) << "Scale: " << scale;
    caffe_scal(size, scale, transformed_data);
  }
}

void rotate(cv::Mat& src, int angle,int flags =cv::INTER_LINEAR,int borderMode = cv::BORDER_CONSTANT,const cv::Scalar& borderValue = cv::Scalar()) {
	// get rotation matrix for rotating the image around its center
	cv::Point2f center(src.cols / 2.0, src.rows / 2.0);
	cv::Mat rot = cv::getRotationMatrix2D(center, angle, 1.0);
	// determine bounding rectangle
	cv::Rect bbox = cv::RotatedRect(center, src.size(), angle).boundingRect();
	// adjust transformation matrix
	rot.at<double>(0, 2) += bbox.width / 2.0 - center.x;
	rot.at<double>(1, 2) += bbox.height / 2.0 - center.y;
	cv::warpAffine(src, src, rot, bbox.size(), flags, borderMode,borderValue);
}

void translation(cv::Mat& src, int translation_x,int translation_y,int flags =cv::INTER_LINEAR,int borderMode = cv::BORDER_CONSTANT,const cv::Scalar& borderValue = cv::Scalar()) {
	cv::Mat t_mat = cv::Mat::zeros(2, 3, CV_32FC1);

	t_mat.at<float>(0, 0) = 1;
	t_mat.at<float>(0, 2) = translation_x; //水平平移量  
	t_mat.at<float>(1, 1) = 1;
	t_mat.at<float>(1, 2) = translation_y; //竖直平移量  
	cv::warpAffine(src, src, t_mat, src.size(),  flags, borderMode,borderValue);
}

template<typename Dtype>
void DataTransformer<Dtype>::TransformImgAndSeg(
    const std::vector<cv::Mat>& cv_img_seg,
    Blob<Dtype>* transformed_data_blob,
    Blob<Dtype>* transformed_label_blob,
	Blob<Dtype>* transformed_edge_blob,
    const int ignore_label) {
  CHECK(cv_img_seg.size() == 3) << "Input must contain image, mask and edge.";

  const int img_channels = cv_img_seg[0].channels();
  // height and width may change due to pad for cropping
  int img_height   = cv_img_seg[0].rows;
  int img_width    = cv_img_seg[0].cols;

  const int seg_channels = cv_img_seg[1].channels();
  int seg_height   = cv_img_seg[1].rows;
  int seg_width    = cv_img_seg[1].cols;

  const int seg_edge_channels = cv_img_seg[2].channels();
  int seg_edge_height = cv_img_seg[2].rows;
  int seg_edge_width = cv_img_seg[2].cols;

  const int data_channels = transformed_data_blob->channels();
  const int data_height   = transformed_data_blob->height();
  const int data_width    = transformed_data_blob->width();

  const int label_channels = transformed_label_blob->channels();
  const int label_height   = transformed_label_blob->height();
  const int label_width    = transformed_label_blob->width();


  const int edge_channels = transformed_edge_blob->channels();
  const int edge_height = transformed_edge_blob->height();
  const int edge_width = transformed_edge_blob->width();

  CHECK_EQ(seg_channels, 1);
  CHECK_EQ(seg_edge_channels, 1);
  CHECK_EQ(img_channels, data_channels);
  CHECK_EQ(img_height, seg_height);
  CHECK_EQ(img_width, seg_width);
  CHECK_EQ(img_height, seg_edge_height);
  CHECK_EQ(img_width, seg_edge_width);

  CHECK_EQ(label_channels, 1);
  CHECK_EQ(data_height, label_height);
  CHECK_EQ(data_width, label_width);
  CHECK_EQ(edge_channels, 1);
  CHECK_EQ(data_height, edge_height);
  CHECK_EQ(data_width, edge_width);


  CHECK(cv_img_seg[0].depth() == CV_8U)
      << "Image data type must be unsigned byte";
  CHECK(cv_img_seg[1].depth() == CV_8U)
      << "Mask data type must be unsigned byte";
  CHECK(cv_img_seg[2].depth() == CV_8U)
	  << "Edge data type must be unsigned byte";

  //const int crop_size = param_.crop_size();
  int crop_width = 0;
  int crop_height = 0;
  CHECK((!param_.has_crop_size() && param_.has_crop_height() && param_.has_crop_width())
	|| (!param_.has_crop_height() && !param_.has_crop_width()))
    << "Must either specify crop_size or both crop_height and crop_width.";
  if (param_.has_crop_size()) {
    crop_width = param_.crop_size();
    crop_height = param_.crop_size();
  } 
  if (param_.has_crop_height() && param_.has_crop_width()) {
    crop_width = param_.crop_width();
    crop_height = param_.crop_height();
  }

  const Dtype scale = param_.scale();
  const bool do_mirror = param_.mirror() && Rand(2); //即使设置flip,也不是一定翻转
   const int translation_value = param_.max_translation();
  const bool do_translation = translation_value > 0;
  const bool has_mean_file = param_.has_mean_file();
  const bool has_mean_values = mean_values_.size() > 0;
  const float max_smooth = param_.max_smooth();
  const float apply_prob = 1.f - param_.apply_probability();
  const bool do_smooth = param_.smooth_filtering() && max_smooth > 1 && Uniform(0.f,1.f) >  apply_prob;
  const int rotation_angle = param_.max_rotation_angle();
  const bool do_rotation = rotation_angle > 0;
  const bool display = param_.display();
  
   
  CHECK_GT(img_channels, 0);

  Dtype* mean = NULL;
  if (has_mean_file) {
    CHECK_EQ(img_channels, data_mean_.channels());
    CHECK_EQ(img_height, data_mean_.height());
    CHECK_EQ(img_width, data_mean_.width());
    mean = data_mean_.mutable_cpu_data();
  }
  if (has_mean_values) {
    CHECK(mean_values_.size() == 1 || mean_values_.size() == img_channels) <<
     "Specify either 1 mean_value or as many as channels: " << img_channels;
    if (img_channels > 1 && mean_values_.size() == 1) {
      // Replicate the mean_value for simplicity
      for (int c = 1; c < img_channels; ++c) {
        mean_values_.push_back(mean_values_[0]);
      }
    }
  }

  // start to perform transformation
  cv::Mat cv_cropped_img;
  cv::Mat cv_cropped_seg;
  cv::Mat cv_cropped_edge;

  if (display )
  {
	  cv::imshow("Source Image", cv_img_seg[0]);
	  cv::waitKey(0);
	  cv::imshow("Source Seg", cv_img_seg[1]);
	  cv::waitKey(0);
	  cv::imshow("Source Edge", cv_img_seg[2]);
	  cv::waitKey(0);
  }
  
  // perform scaling
  if (scale_factors_.size() > 0) 
  {
    int scale_ind = Rand(scale_factors_.size());
    Dtype scale_   = scale_factors_[scale_ind];

    if (scale_ != 1) 
	{
      img_height *= scale_;
      img_width  *= scale_;
      cv::resize(cv_img_seg[0], cv_cropped_img, cv::Size(img_width, img_height), 0, 0, 
		 cv::INTER_LINEAR);
      cv::resize(cv_img_seg[1], cv_cropped_seg, cv::Size(img_width, img_height), 0, 0, 
		 cv::INTER_NEAREST);
	  cv::resize(cv_img_seg[2], cv_cropped_edge, cv::Size(img_width, img_height), 0, 0,
		  cv::INTER_NEAREST);
	 if (display )
		{
			cv::imshow("Scaled Image", cv_cropped_img);
			cv::waitKey(0);
			cv::imshow("Scaled Seg", cv_cropped_seg);
			cv::waitKey(0);
			cv::imshow("Scaled Edge", cv_cropped_edge);
			cv::waitKey(0);
		}	  
    }
	else 
	{
      cv_cropped_img = cv_img_seg[0];
      cv_cropped_seg = cv_img_seg[1];
	  cv_cropped_edge = cv_img_seg[2];
    }
  }
  else
  {
    cv_cropped_img = cv_img_seg[0];
    cv_cropped_seg = cv_img_seg[1];
	cv_cropped_edge = cv_img_seg[2];
  }
  // set smoothness
  int smooth_param = 0;
  int smooth_type = 0;
  if (do_smooth) {
    smooth_type = Rand(5);
    smooth_param = 1 + 2 * Rand(max_smooth/2);
    switch (smooth_type) {
        case 0:
            cv::GaussianBlur(cv_cropped_img, cv_cropped_img, cv::Size(smooth_param, smooth_param), 0);
            break;
        case 1:
            cv::blur(cv_cropped_img, cv_cropped_img, cv::Size(smooth_param, smooth_param));
            break;
        case 2:
            cv::medianBlur(cv_cropped_img, cv_cropped_img, smooth_param);
            break;
        case 3:
            cv::boxFilter(cv_cropped_img, cv_cropped_img, -1, cv::Size(smooth_param * 2, smooth_param * 2));
            break;
        default:
            break;
    }
	if (display )
		{
			cv::imshow("Smooth Filtering Image", cv_cropped_img);
			cv::waitKey(0);
			cv::imshow("Smooth Filtering Seg", cv_cropped_seg);
			cv::waitKey(0);
			cv::imshow("Smooth Filtering Edge", cv_cropped_edge);
			cv::waitKey(0);
		}
  }
 


  // transform to double, since we will pad mean pixel values
  cv_cropped_img.convertTo(cv_cropped_img, CV_64F);
  
   //perform translation
  int current_translation_x  = 0;
  int current_translation_y  = 0;
  if(do_translation)
  {
	  current_translation_x = Rand(translation_value*2 + 1) - translation_value;
	  current_translation_y = Rand(translation_value*2 + 1) - translation_value;
	  if (current_translation_x||current_translation_y)
	{
	    if(has_mean_values)
		   {
			   translation(cv_cropped_img, current_translation_x,current_translation_y,cv::INTER_LINEAR,cv::BORDER_CONSTANT,
			  cv::Scalar(mean_values_[0], mean_values_[1], mean_values_[2]));
		   }
		  else
			{
			    translation(cv_cropped_img, current_translation_x,current_translation_y,cv::INTER_LINEAR,cv::BORDER_REFLECT_101);
			}
		 translation(cv_cropped_seg, current_translation_x,current_translation_y, cv::INTER_NEAREST,cv::BORDER_CONSTANT,
			    cv::Scalar(ignore_label));
	     translation(cv_cropped_edge, current_translation_x,current_translation_y, cv::INTER_NEAREST,cv::BORDER_CONSTANT,
			    cv::Scalar(ignore_label));			
				
	    if (display )
		{
			cv::Mat temp;
			cv_cropped_img.convertTo(temp, CV_8U);
			cv::imshow("Translation Image", temp);
			cv::waitKey(0);
			cv::imshow("Translation Seg", cv_cropped_seg);
			cv::waitKey(0);
			cv::imshow("Translation Edge", cv_cropped_edge);
			cv::waitKey(0);
		}		
	  }
	  
  }
  
   //perform rotate
   int current_angle = 0;
  if (do_rotation) {
    current_angle = Rand(rotation_angle*2 + 1) - rotation_angle;
    if (current_angle)
	{
	    if(has_mean_values)
		   {
			   rotate(cv_cropped_img, current_angle,cv::INTER_LINEAR,cv::BORDER_CONSTANT,
			  cv::Scalar(mean_values_[0], mean_values_[1], mean_values_[2]));
		   }
		  else
			{
			    rotate(cv_cropped_img, current_angle,cv::INTER_LINEAR,cv::BORDER_REFLECT_101);
			}
		 rotate(cv_cropped_seg, current_angle, cv::INTER_NEAREST,cv::BORDER_CONSTANT,
			    cv::Scalar(ignore_label));
				
		 rotate(cv_cropped_edge, current_angle, cv::INTER_NEAREST,cv::BORDER_CONSTANT,
			    cv::Scalar(ignore_label));
	    if (display )
		{
			cv::Mat temp;
			cv_cropped_img.convertTo(temp, CV_8U);
			cv::imshow("Rotation Image", temp);
			cv::waitKey(0);
			cv::imshow("Rotation Seg", cv_cropped_seg);
			cv::waitKey(0);
			cv::imshow("Rotation Edge", cv_cropped_edge);
			cv::waitKey(0);
		}		
	  }
  }
  // update height/width due to rotate
    img_height   = cv_cropped_img.rows;
    img_width    = cv_cropped_img.cols;

    seg_height   = cv_cropped_seg.rows;
    seg_width    = cv_cropped_seg.cols;

	seg_edge_height = cv_cropped_edge.rows;
	seg_edge_width = cv_cropped_edge.cols;
	
    CHECK_EQ(img_height, seg_height);
    CHECK_EQ(img_width, seg_width);
    CHECK_EQ(img_height, seg_edge_height);
    CHECK_EQ(img_width, seg_edge_width);

	
  int h_off = 0;
  int w_off = 0;	
  // Check if we need to pad img to fit for crop_size
  // copymakeborder
  int pad_height = std::max(crop_height - img_height, 0);
  int pad_width  = std::max(crop_width - img_width, 0);
  if (pad_height > 0 || pad_width > 0)
  {
	  if(has_mean_values)
	   {
		    cv::copyMakeBorder(cv_cropped_img, cv_cropped_img, 0, pad_height,
          0, pad_width, cv::BORDER_CONSTANT,
          cv::Scalar(mean_values_[0], mean_values_[1], mean_values_[2]));
	   }
	  else
		{
	    cv::copyMakeBorder(cv_cropped_img, cv_cropped_img, 0, pad_height,
          0, pad_width, cv::BORDER_REFLECT_101);
		}
    cv::copyMakeBorder(cv_cropped_seg, cv_cropped_seg, 0, pad_height,
          0, pad_width, cv::BORDER_CONSTANT,
          cv::Scalar(ignore_label));
	cv::copyMakeBorder(cv_cropped_edge, cv_cropped_edge, 0, pad_height,
		0, pad_width, cv::BORDER_CONSTANT,
		cv::Scalar(ignore_label));
    // update height/width
    img_height   = cv_cropped_img.rows;
    img_width    = cv_cropped_img.cols;

    seg_height   = cv_cropped_seg.rows;
    seg_width    = cv_cropped_seg.cols;

	seg_edge_height = cv_cropped_edge.rows;
	seg_edge_width = cv_cropped_edge.cols;
	
    CHECK_EQ(img_height, seg_height);
    CHECK_EQ(img_width, seg_width);
    CHECK_EQ(img_height, seg_edge_height);
    CHECK_EQ(img_width, seg_edge_width);
  }

  // crop img/seg
  if (crop_width && crop_height)
  {
    CHECK_EQ(crop_height, data_height);
    CHECK_EQ(crop_width, data_width);
    // We only do random crop when we do training.
    if (phase_ == TRAIN)
	{
      h_off = Rand(img_height - crop_height + 1);
      w_off = Rand(img_width - crop_width + 1);
    }
	else
	{
      // CHECK: use middle crop
      h_off = (img_height - crop_height) / 2;
      w_off = (img_width - crop_width) / 2;
    }
    cv::Rect roi(w_off, h_off, crop_width, crop_height);
    cv_cropped_img = cv_cropped_img(roi);
    cv_cropped_seg = cv_cropped_seg(roi);
	cv_cropped_edge = cv_cropped_edge(roi);
	if (display )
		{
			cv::Mat temp;
			cv_cropped_img.convertTo(temp, CV_8U);
			cv::imshow("Cropping Image", temp);
			cv::waitKey(0);
			cv::imshow("Cropping Seg", cv_cropped_seg);
			cv::waitKey(0);
			cv::imshow("Cropping Edge", cv_cropped_edge);
			cv::waitKey(0);
		}	
  }

  CHECK(cv_cropped_img.data);
  CHECK(cv_cropped_seg.data);
  CHECK(cv_cropped_edge.data);

  
  //赋值
  Dtype* transformed_data  = transformed_data_blob->mutable_cpu_data();
  Dtype* transformed_label = transformed_label_blob->mutable_cpu_data();
  Dtype* transformed_edge = transformed_edge_blob->mutable_cpu_data();

  int top_index;
  const double* data_ptr;
  const uchar* label_ptr;
  const uchar* edge_ptr;

  for (int h = 0; h < data_height; ++h) 
  {
    data_ptr = cv_cropped_img.ptr<double>(h);
    label_ptr = cv_cropped_seg.ptr<uchar>(h);
	edge_ptr = cv_cropped_edge.ptr<uchar>(h);

    int data_index = 0;
    int label_index = 0;
	int edge_index = 0;

    for (int w = 0; w < data_width; ++w) 
	{
		  // for image
		  for (int c = 0; c < img_channels; ++c)
		  {
				if (do_mirror) 
				{
				  top_index = (c * data_height + h) * data_width + (data_width - 1 - w);
				} 
				else 
				{
				  top_index = (c * data_height + h) * data_width + w;
				}

				Dtype pixel = static_cast<Dtype>(data_ptr[data_index++]);
				if (has_mean_file) 
				{
				  int mean_index = (c * img_height + h_off + h) * img_width + w_off + w;
				  transformed_data[top_index] =(pixel - mean[mean_index]) * scale;
				} 
				else 
				{
				  if (has_mean_values) 
				  {
					transformed_data[top_index] = (pixel - mean_values_[c]) * scale;
				  } 
				  else
				  {
					transformed_data[top_index] = pixel * scale;
				  }
				}
		  }

		  // for segmentation
		  if (do_mirror) 
		  {
			top_index = h * data_width + data_width - 1 - w;
		  } 
		  else
		  {
			top_index = h * data_width + w;
		  }
		  Dtype pixel = static_cast<Dtype>(label_ptr[label_index++]);
		  transformed_label[top_index] = pixel;

		  Dtype pixel_edge = static_cast<Dtype>(edge_ptr[edge_index++]);
		  transformed_edge[top_index] = pixel_edge;
	}
		
  }
}

template<typename Dtype>
vector<int> DataTransformer<Dtype>::InferBlobShape(const Datum& datum) {
  if (datum.encoded()) {
#ifdef USE_OPENCV
    CHECK(!(param_.force_color() && param_.force_gray()))
        << "cannot set both force_color and force_gray";
    cv::Mat cv_img;
    if (param_.force_color() || param_.force_gray()) {
    // If force_color then decode in color otherwise decode in gray.
      cv_img = DecodeDatumToCVMat(datum, param_.force_color());
    } else {
      cv_img = DecodeDatumToCVMatNative(datum);
    }
    // InferBlobShape using the cv::image.
    return InferBlobShape(cv_img);
#else
    LOG(FATAL) << "Encoded datum requires OpenCV; compile with USE_OPENCV.";
#endif  // USE_OPENCV
  }
  const int crop_size = param_.crop_size();
  const int datum_channels = datum.channels();
  const int datum_height = datum.height();
  const int datum_width = datum.width();
  // Check dimensions.
  CHECK_GT(datum_channels, 0);
  CHECK_GE(datum_height, crop_size);
  CHECK_GE(datum_width, crop_size);
  // Build BlobShape.
  vector<int> shape(4);
  shape[0] = 1;
  shape[1] = datum_channels;
  shape[2] = (crop_size)? crop_size: datum_height;
  shape[3] = (crop_size)? crop_size: datum_width;
  return shape;
}

template<typename Dtype>
vector<int> DataTransformer<Dtype>::InferBlobShape(
    const vector<Datum> & datum_vector) {
  const int num = datum_vector.size();
  CHECK_GT(num, 0) << "There is no datum to in the vector";
  // Use first datum in the vector to InferBlobShape.
  vector<int> shape = InferBlobShape(datum_vector[0]);
  // Adjust num to the size of the vector.
  shape[0] = num;
  return shape;
}

#ifdef USE_OPENCV
template<typename Dtype>
vector<int> DataTransformer<Dtype>::InferBlobShape(const cv::Mat& cv_img) {
  const int crop_size = param_.crop_size();
  const int img_channels = cv_img.channels();
  const int img_height = cv_img.rows;
  const int img_width = cv_img.cols;
  // Check dimensions.
  CHECK_GT(img_channels, 0);
  CHECK_GE(img_height, crop_size);
  CHECK_GE(img_width, crop_size);
  // Build BlobShape.
  vector<int> shape(4);
  shape[0] = 1;
  shape[1] = img_channels;
  shape[2] = (crop_size)? crop_size: img_height;
  shape[3] = (crop_size)? crop_size: img_width;
  return shape;
}
template<typename Dtype>
vector<int> DataTransformer<Dtype>::InferBlobShapeAboutCrop(const cv::Mat& cv_img) {
	int crop_width = 0;
	int crop_height = 0;
	//或者指定其一，或者都不指定
	CHECK((!param_.has_crop_size() && param_.has_crop_height() && param_.has_crop_width())
		|| (!param_.has_crop_height() && !param_.has_crop_width()))
		<< "Must either specify crop_size or both crop_height and crop_width.";
	if (param_.has_crop_size()) {
		crop_width = param_.crop_size();
		crop_height = param_.crop_size();
	}
	if (param_.has_crop_height() && param_.has_crop_width()) {
		crop_width = param_.crop_width();
		crop_height = param_.crop_height();
	}
	const int img_channels = cv_img.channels();
	const int img_height = cv_img.rows;
	const int img_width = cv_img.cols;
	// Check dimensions.
	CHECK_GT(img_channels, 0);
	// Build BlobShape.
	vector<int> shape(4);
	shape[0] = 1;
	shape[1] = img_channels;
	if (crop_width > 0 && crop_height > 0)
	{
		shape[2] = crop_height;
		shape[3] = crop_width;
	}
	else
	{
		shape[2] = img_height;
		shape[3] = img_width;	
	}
	return shape;
}
template<typename Dtype>
vector<int> DataTransformer<Dtype>::InferBlobShape(
    const vector<cv::Mat> & mat_vector) {
  const int num = mat_vector.size();
  CHECK_GT(num, 0) << "There is no cv_img to in the vector";
  // Use first cv_img in the vector to InferBlobShape.
  vector<int> shape = InferBlobShape(mat_vector[0]);
  // Adjust num to the size of the vector.
  shape[0] = num;
  return shape;
}
#endif  // USE_OPENCV

template <typename Dtype>
void DataTransformer<Dtype>::InitRand() {
  const float max_smooth = param_.max_smooth();
  const bool do_smooth = param_.smooth_filtering() && max_smooth > 1;
  
  const bool needs_rand = param_.mirror() ||do_smooth||
      (phase_ == TRAIN && param_.crop_size())||(phase_ == TRAIN && param_.crop_width()&&param_.crop_height());
  if (needs_rand) {
    const unsigned int rng_seed = caffe_rng_rand();
    rng_.reset(new Caffe::RNG(rng_seed));
  } else {
    rng_.reset();
  }
}

template <typename Dtype>
int DataTransformer<Dtype>::Rand(int n) {
  CHECK(rng_);
  CHECK_GT(n, 0);
  caffe::rng_t* rng =
      static_cast<caffe::rng_t*>(rng_->generator());
  return ((*rng)() % n);
}

template <typename Dtype>
float DataTransformer<Dtype>::Uniform(const float min, const float max) {
  CHECK(rng_);
  Dtype d[1];
  caffe_rng_uniform<Dtype>(1, Dtype(min), Dtype(max), d);
  return (float)d[0];
}


INSTANTIATE_CLASS(DataTransformer);

}  // namespace caffe
