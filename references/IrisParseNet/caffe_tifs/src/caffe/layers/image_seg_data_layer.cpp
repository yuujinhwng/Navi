#include <fstream>  // NOLINT(readability/streams)
#include <iostream>  // NOLINT(readability/streams)
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/imgproc/imgproc.hpp>

#include "caffe/data_transformer.hpp"
#include "caffe/layers/base_data_layer.hpp"
#include "caffe/layers/image_seg_data_layer.hpp"
#include "caffe/util/benchmark.hpp"
#include "caffe/util/io.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/util/rng.hpp"




namespace caffe {

template <typename Dtype>
ImageSegDataLayer<Dtype>::~ImageSegDataLayer<Dtype>() {
  this->StopInternalThread();
}

template <typename Dtype>
void ImageSegDataLayer<Dtype>::DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  const int new_height = this->layer_param_.image_data_param().new_height();    //默认为0
  const int new_width  = this->layer_param_.image_data_param().new_width();   //默认为0
  const bool is_color  = this->layer_param_.image_data_param().is_color();      //默认为1
  const int label_type = this->layer_param_.image_data_param().label_type();     //默认为IMAGE   1 ，PIXEL 2 ，NONE 0 
  string root_folder = this->layer_param_.image_data_param().root_folder();

  TransformationParameter transform_param = this->layer_param_.transform_param();
  CHECK(transform_param.has_mean_file() == false) << 
         "ImageSegDataLayer does not support mean file";
  CHECK((new_height == 0 && new_width == 0) ||
      (new_height > 0 && new_width > 0)) << "Current implementation requires "
      "new_height and new_width to be set at the same time.";


  // Read the file with filenames and labels
  const string& source = root_folder+this->layer_param_.image_data_param().source();
  LOG(INFO) << "Opening file " << source;
  std::ifstream infile(source.c_str());

  string linestr;
  while (std::getline(infile, linestr)) {
    std::istringstream iss(linestr);
    string imgfn;
    iss >> imgfn;
    string maskfn = ""; //代表mask
	string edgefn = ""; //代表edge
    if (label_type != ImageDataParameter_LabelType_NONE) 
	{
		
	   iss >> maskfn>> edgefn;
	
    }
    lines_.push_back(Data_Label_Edge(imgfn, maskfn, edgefn));
	 
  }
  CHECK(!lines_.empty()) << "File is empty";

  if (this->layer_param_.image_data_param().shuffle()) {
    // randomly shuffle data
    LOG(INFO) << "Shuffling data";
    const unsigned int prefetch_rng_seed = caffe_rng_rand();
    prefetch_rng_.reset(new Caffe::RNG(prefetch_rng_seed));
    ShuffleImages();
  }
  else {
	  if (this->phase_ == TRAIN && Caffe::solver_rank() > 0 &&
		  this->layer_param_.image_data_param().rand_skip() == 0) {
		  LOG(WARNING) << "Shuffling or skipping recommended for multi-GPU";
	  }
  }

  LOG(INFO) << "A total of " << lines_.size() << " images.";

  lines_id_ = 0;
  // Check if we would need to randomly skip a few data points
  if (this->layer_param_.image_data_param().rand_skip()) {
    unsigned int skip = caffe_rng_rand() %
        this->layer_param_.image_data_param().rand_skip();
    LOG(INFO) << "Skipping first " << skip << " data points.";
    CHECK_GT(lines_.size(), skip) << "Not enough points to skip";
    lines_id_ = skip;
  }
  // Read an image, and use it to initialize the top blob.
  cv::Mat cv_img = ReadImageToCVMat(root_folder + lines_[lines_id_].data,
                                    new_height, new_width, is_color);


  CHECK(cv_img.data) << "Could not load " << lines_[lines_id_].data;
  // Use data_transformer to infer the expected blob shape from a cv_image.
  vector<int> top_shape = this->data_transformer_->InferBlobShapeAboutCrop(cv_img);
   const int batch_size = this->layer_param_.image_data_param().batch_size();
  CHECK_GT(batch_size, 0) << "Positive batch size required";

  //data
   this->transformed_data_.Reshape(top_shape);
   top_shape[0] = batch_size;
	top[0]->Reshape(top_shape);
	for (int i = 0; i < this->prefetch_.size(); ++i) {
		this->prefetch_[i]->data_.Reshape(top_shape);
	}

	//mask
	top_shape[0] = 1;
	top_shape[1] = 1;
	this->transformed_labelmap_.Reshape(top_shape);
	top_shape[0] = batch_size;
	top[1]->Reshape(top_shape);
	for (int i = 0; i < this->prefetch_.size(); ++i) {
		this->prefetch_[i]->labelmap_.Reshape(top_shape);
	}
	//edge
	top_shape[0] = 1;
	this->transformed_labeledge_.Reshape(top_shape);
	top_shape[0] = batch_size;
	top[2]->Reshape(top_shape);
	
	for (int i = 0; i < this->prefetch_.size(); ++i) {
		this->prefetch_[i]->labeledge_.Reshape(top_shape);
	}


  LOG(INFO) << "output data size: " << top[0]->num() << ","
	    << top[0]->channels() << "," << top[0]->height() << ","
	    << top[0]->width();
  // label
  LOG(INFO) << "output mask size: " << top[1]->num() << ","
	    << top[1]->channels() << "," << top[1]->height() << ","
	    << top[1]->width();
  // edge
  
	LOG(INFO) << "output edge size: " << top[2]->num() << ","
		<< top[2]->channels() << "," << top[2]->height() << ","
		<< top[2]->width();
 
}

template <typename Dtype>
void ImageSegDataLayer<Dtype>::ShuffleImages() {
  caffe::rng_t* prefetch_rng =
      static_cast<caffe::rng_t*>(prefetch_rng_->generator());
  shuffle(lines_.begin(), lines_.end(), prefetch_rng);
}

// This function is called on prefetch thread
template <typename Dtype>
void ImageSegDataLayer<Dtype>::load_batch(LabelmapBatch<Dtype>* batch) {
  CPUTimer batch_timer;
  batch_timer.Start();
  double read_time = 0;
  double trans_time = 0;
  CPUTimer timer;
  CHECK(batch->data_.count());
  CHECK(batch->labelmap_.count());
  CHECK(batch->labeledge_.count());
  CHECK(this->transformed_data_.count());
  CHECK(this->transformed_labelmap_.count());
  CHECK(this->transformed_labeledge_.count());


  ImageDataParameter image_data_param = this->layer_param_.image_data_param();
  const int batch_size = image_data_param.batch_size();
  CHECK_GT(batch_size, 0) << "Positive batch size required";
  const int new_height = image_data_param.new_height();
  const int new_width  = image_data_param.new_width();
  const int label_type = this->layer_param_.image_data_param().label_type();
  const int ignore_label = image_data_param.ignore_label();
  const bool is_color  = image_data_param.is_color();
  string root_folder   = image_data_param.root_folder();

  // Reshape according to the first image of each batch
  // on single input batches allows for inputs of varying dimension.

  cv::Mat cv_img = ReadImageToCVMat(root_folder + lines_[lines_id_].data,
	  new_height, new_width, is_color);
  CHECK(cv_img.data) << "Could not load " << lines_[lines_id_].data;
  // Use data_transformer to infer the expected blob shape from a cv_img.
  vector<int> top_shape = this->data_transformer_->InferBlobShapeAboutCrop(cv_img);

  this->transformed_data_.Reshape(top_shape);
  top_shape[0] = batch_size;
  batch->data_.Reshape(top_shape);

  top_shape[0] = 1;
  top_shape[1] = 1;
  this->transformed_labelmap_.Reshape(top_shape);
  this->transformed_labeledge_.Reshape(top_shape);
  top_shape[0] = batch_size;
  batch->labelmap_.Reshape(top_shape);
  batch->labeledge_.Reshape(top_shape);

  

  Dtype* top_data = batch->data_.mutable_cpu_data();
  Dtype* top_labelmap = batch->labelmap_.mutable_cpu_data();
  Dtype* top_labeledge = batch->labeledge_.mutable_cpu_data();

  const int lines_size = lines_.size();
  
  for (int item_id = 0; item_id < batch_size; ++item_id) 
  {
		std::vector<cv::Mat> cv_img_seg;

		// get a blob
		timer.Start();
		CHECK_GT(lines_size, lines_id_);

   
		cv_img_seg.push_back(ReadImageToCVMat(root_folder + lines_[lines_id_].data,
		  new_height, new_width, is_color));


		if (!cv_img_seg[0].data) 
		{
		  DLOG(INFO) << "Fail to load img: " << root_folder + lines_[lines_id_].data;
		}
		//mask  
		if (label_type == ImageDataParameter_LabelType_PIXEL) 
		{
		  cv_img_seg.push_back(ReadImageToCVMat(root_folder + lines_[lines_id_].label,
							new_height, new_width, false));
		  if (!cv_img_seg[1].data) 
		  {
			DLOG(INFO) << "Fail to load mask: " << root_folder + lines_[lines_id_].label;
		  }
		  //edge
		  cv_img_seg.push_back(ReadImageToCVMat(root_folder + lines_[lines_id_].edge,
			  new_height, new_width, false));
		  if (!cv_img_seg[2].data) 
		  {
			  DLOG(INFO) << "Fail to load edge: " << root_folder + lines_[lines_id_].edge;
		  }
		}
		else if (label_type == ImageDataParameter_LabelType_IMAGE)
		{
		  const int label = atoi(lines_[lines_id_].label.c_str());
		  cv::Mat seg(cv_img_seg[0].rows, cv_img_seg[0].cols, 
			  CV_8UC1, cv::Scalar(label));
		  cv_img_seg.push_back(seg);     
		  //edge
		  const int edge = atoi(lines_[lines_id_].edge.c_str());
		  cv::Mat seg2(cv_img_seg[0].rows, cv_img_seg[0].cols,
			  CV_8UC1, cv::Scalar(edge));
		  cv_img_seg.push_back(seg2);

		}
		else 
		{
		  cv::Mat seg(cv_img_seg[0].rows, cv_img_seg[0].cols, 
			  CV_8UC1, cv::Scalar(ignore_label));
		  cv_img_seg.push_back(seg);
		  //edge
		  cv_img_seg.push_back(seg);

		}


		read_time += timer.MicroSeconds();
		timer.Start();
		// Apply transformations (mirror, crop...) to the image
		int offset;
		offset = batch->data_.offset(item_id);
		this->transformed_data_.set_cpu_data(top_data + offset);

		offset = batch->labelmap_.offset(item_id);
		this->transformed_labelmap_.set_cpu_data(top_labelmap + offset);

		offset = batch->labeledge_.offset(item_id);
		this->transformed_labeledge_.set_cpu_data(top_labeledge + offset);

		this->data_transformer_->TransformImgAndSeg(cv_img_seg, 
		 &(this->transformed_data_), &(this->transformed_labelmap_), &(this->transformed_labeledge_),
		 ignore_label);
		trans_time += timer.MicroSeconds();

		// go to the next std::vector<int>::iterator iter;
		lines_id_++;
		if (lines_id_ >= lines_size) 
		{
		  // We have reached the end. Restart from the first.
		  DLOG(INFO) << "Restarting data prefetching from start.";
		  lines_id_ = 0;
		  if (this->layer_param_.image_data_param().shuffle())
		  {
			 ShuffleImages();
		  }
		}
  }
  batch_timer.Stop();
  DLOG(INFO) << "Prefetch batch: " << batch_timer.MilliSeconds() << " ms.";
  DLOG(INFO) << "     Read time: " << read_time / 1000 << " ms.";
  DLOG(INFO) << "Transform time: " << trans_time / 1000 << " ms.";
}

INSTANTIATE_CLASS(ImageSegDataLayer);
REGISTER_LAYER_CLASS(ImageSegData);

}  // namespace caffe
