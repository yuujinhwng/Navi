#ifndef CAFFE_DATA_LAYERS_HPP_
#define CAFFE_DATA_LAYERS_HPP_

#include <vector>

#include "caffe/blob.hpp"
#include "caffe/data_transformer.hpp"
#include "caffe/internal_thread.hpp"
#include "caffe/layer.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/util/blocking_queue.hpp"

namespace caffe {

/**
* @brief Provides base for data layers that feed blobs to the Net.
*
* TODO(dox): thorough documentation for Forward and proto params.
*/
template <typename Dtype>
class BaseDataLayer : public Layer<Dtype> {
public:
	explicit BaseDataLayer(const LayerParameter& param);
	// LayerSetUp: implements common data layer setup functionality, and calls
	// DataLayerSetUp to do special data layer setup for individual layer types.
	// This method may not be overridden except by the BasePrefetchingDataLayer.
	virtual void LayerSetUp(const vector<Blob<Dtype>*>& bottom,
		const vector<Blob<Dtype>*>& top);
	virtual void DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
		const vector<Blob<Dtype>*>& top) {}
	// Data layers have no bottoms, so reshaping is trivial.
	virtual void Reshape(const vector<Blob<Dtype>*>& bottom,
		const vector<Blob<Dtype>*>& top) {}

	virtual void Backward_cpu(const vector<Blob<Dtype>*>& top,
		const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {}
	virtual void Backward_gpu(const vector<Blob<Dtype>*>& top,
		const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {}

protected:
	TransformationParameter transform_param_;
	shared_ptr<DataTransformer<Dtype> > data_transformer_;
	bool output_labels_;
};

template <typename Dtype>
class Batch {
public:
	Blob<Dtype> data_, label_;
};

template <typename Dtype>
class LabelmapBatch {
public:
	Blob<Dtype> data_, labelmap_, labeledge_;
};


template <typename Dtype>
class BasePrefetchingDataLayer :
	public BaseDataLayer<Dtype>, public InternalThread {
public:
	explicit BasePrefetchingDataLayer(const LayerParameter& param);
	// LayerSetUp: implements common data layer setup functionality, and calls
	// DataLayerSetUp to do special data layer setup for individual layer types.
	// This method may not be overridden.
	void LayerSetUp(const vector<Blob<Dtype>*>& bottom,
		const vector<Blob<Dtype>*>& top);

	virtual void Forward_cpu(const vector<Blob<Dtype>*>& bottom,
		const vector<Blob<Dtype>*>& top);
	virtual void Forward_gpu(const vector<Blob<Dtype>*>& bottom,
		const vector<Blob<Dtype>*>& top);

protected:
	virtual void InternalThreadEntry();
	virtual void load_batch(Batch<Dtype>* batch) = 0;

	vector<shared_ptr<Batch<Dtype> > > prefetch_;
	BlockingQueue<Batch<Dtype>*> prefetch_free_;
	BlockingQueue<Batch<Dtype>*> prefetch_full_;
	Batch<Dtype>* prefetch_current_;

	Blob<Dtype> transformed_data_;
};


template <typename Dtype>
class BasePrefetchingLabelmapDataLayer :
	public BaseDataLayer<Dtype>, public InternalThread {
public:
	explicit BasePrefetchingLabelmapDataLayer(const LayerParameter& param);
	// LayerSetUp: implements common data layer setup functionality, and calls
	// DataLayerSetUp to do special data layer setup for individual layer types.
	// This method may not be overridden.
	void LayerSetUp(const vector<Blob<Dtype>*>& bottom,
		const vector<Blob<Dtype>*>& top);

	virtual void Forward_cpu(const vector<Blob<Dtype>*>& bottom,
		const vector<Blob<Dtype>*>& top);
	virtual void Forward_gpu(const vector<Blob<Dtype>*>& bottom,
		const vector<Blob<Dtype>*>& top);


protected:
	virtual void InternalThreadEntry();
	virtual void load_batch(LabelmapBatch<Dtype>* labelmapbatch) = 0;

	
	vector<shared_ptr<LabelmapBatch<Dtype> > > prefetch_;
	BlockingQueue<LabelmapBatch<Dtype>*> prefetch_free_;
	BlockingQueue<LabelmapBatch<Dtype>*> prefetch_full_;
	LabelmapBatch<Dtype>* prefetch_current_;


	Blob<Dtype> transformed_data_;   //实际上指一幅图像（1，c,h,w）
	Blob<Dtype> transformed_labelmap_;
	Blob<Dtype> transformed_labeledge_;
	bool output_edges_;   //是否输入edge
};


}  // namespace caffe

#endif  // CAFFE_DATA_LAYERS_HPP_