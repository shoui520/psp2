/*
 * The operations Image runs to make a new image from one: each an
 * ImageOperation with its parameters. Declared as far as Image uses them.
 */
#ifndef PAF_IMAGE_IMAGE_OPERATION_H
#define PAF_IMAGE_IMAGE_OPERATION_H

#include <image/image.h>

namespace paf {
namespace image {

class CopyOperation : public ImageOperation {
public:
	CopyOperation(memory::HeapAllocator *allocator);

	virtual ~CopyOperation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);
};

class ConvertToRGBA8888Operation : public ImageOperation {
public:
	ConvertToRGBA8888Operation(ImageConvertModeType type, memory::HeapAllocator *allocator);

	virtual ~ConvertToRGBA8888Operation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	ImageConvertModeType m_type;
};

class ConvertToDXT1Operation : public ImageOperation {
public:
	ConvertToDXT1Operation(ImageConvertModeType type, memory::HeapAllocator *allocator);

	virtual ~ConvertToDXT1Operation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	ImageConvertModeType m_type;
};

class ConvertToDXT3Operation : public ImageOperation {
public:
	ConvertToDXT3Operation(ImageConvertModeType type, memory::HeapAllocator *allocator);

	virtual ~ConvertToDXT3Operation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	ImageConvertModeType m_type;
};

class ConvertToDXT5Operation : public ImageOperation {
public:
	ConvertToDXT5Operation(ImageConvertModeType type, memory::HeapAllocator *allocator);

	virtual ~ConvertToDXT5Operation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	ImageConvertModeType m_type;
};

class PasteOperation : public ImageOperation {
public:
	PasteOperation(common::SharedPtr<Image> image, const ImagePoint &point,
		       memory::HeapAllocator *allocator);

	virtual ~PasteOperation();
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	common::SharedPtr<Image> m_image;
	ImagePoint m_point;
};

class CropOperation : public ImageOperation {
public:
	CropOperation(const ImageRect &crop, memory::HeapAllocator *allocator);

	virtual ~CropOperation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	ImageRect m_crop;
};

class CopyWithEdgeOperation : public ImageOperation {
public:
	CopyWithEdgeOperation(uint32_t extlines, memory::HeapAllocator *allocator);

	virtual ~CopyWithEdgeOperation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	uint32_t m_extlines;
};

class FadeWithEdgeOperation : public ImageOperation {
public:
	FadeWithEdgeOperation(memory::HeapAllocator *allocator);

	virtual ~FadeWithEdgeOperation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);
};

class ResizeOperation : public ImageOperation {
public:
	ResizeOperation(const ImageExtent &extent, ImageResizeType type,
			memory::HeapAllocator *allocator);

	virtual ~ResizeOperation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	ImageExtent m_extent;
	ImageResizeType m_type;
};

class ResizeWithConvertModeOperation : public ImageOperation {
public:
	ResizeWithConvertModeOperation(const ImageExtent &extent, ImageResizeType resize_type,
				       ImageMode mode, ImageConvertModeType type,
				       memory::HeapAllocator *allocator);

	virtual ~ResizeWithConvertModeOperation();
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	ImageExtent m_extent;
	ImageResizeType m_resize_type;
	ImageMode m_mode;
	ImageConvertModeType m_type;
};

class RotateOperation : public ImageOperation {
public:
	RotateOperation(ImageRotateType type, memory::HeapAllocator *allocator);

	virtual ~RotateOperation();
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	ImageRotateType m_type;
};

class FlipOperation : public ImageOperation {
public:
	FlipOperation(ImageFlipType type, memory::HeapAllocator *allocator);

	virtual ~FlipOperation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	ImageFlipType m_type;
};

class BlurOperation : public ImageOperation {
public:
	BlurOperation(float sigma, memory::HeapAllocator *allocator);

	virtual ~BlurOperation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	float m_sigma;
};

class SharpenOperation : public ImageOperation {
public:
	SharpenOperation(float amount, int32_t threshold, memory::HeapAllocator *allocator);

	virtual ~SharpenOperation();
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	float m_amount;
	int32_t m_threshold;
};

class TransparentizeOperation : public ImageOperation {
public:
	TransparentizeOperation(float alpha, memory::HeapAllocator *allocator);

	virtual ~TransparentizeOperation();
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	float m_alpha;
};

class RoundCornerOperation : public ImageOperation {
public:
	RoundCornerOperation(int32_t width, int32_t height, memory::HeapAllocator *allocator);

	virtual ~RoundCornerOperation();
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	int32_t m_width;
	int32_t m_height;
};

class BlendOperation : public ImageOperation {
public:
	BlendOperation(common::SharedPtr<Image> image, float alpha,
		       memory::HeapAllocator *allocator);

	virtual ~BlendOperation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut);

private:
	common::SharedPtr<Image> m_image;
	float m_alpha;
};

}
}

#endif
