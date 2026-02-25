#ifndef VIDEO_FRAME_H
#define VIDEO_FRAME_H

#include <cstdint>
#include <vector>

class VideoFrame {
    public:
    //图片数据格式
    enum class Format {
        YUV420,   // YUV420格式
        YUV422,   // YUV422格式
        NV12,     // NV12格式
        RGB24     // RGB24格式
        //可以根据需求添加其他格式
    };
    //图像数据缓冲区，使用std::vector来管理
    std::vector<uint8_t> data;

    //视频帧的宽度和高度
    int width;
    int height;

    //帧的时间戳
    uint64_t timestamp; 
    //图片数据格式
    Format format;

    //默认构造函数
    VideoFrame (int w, int h, Format fmt = Format::YUV420) : width(w), height(h), format(fmt) {
        resizeData();
    }

    //设置时间戳
    void setTimestamp(uint64_t ts) {
        timestamp = ts;
    }

    //获取数据大小
    size_t getDataSize() const {
        return data.size();
    }

    //获取图像格式的字符串表示
    std::string getFormatString() const {
        switch (format) {
            case Format::YUV420:
                return "YUV420";
            case Format::YUV422:
                return "YUV422";
            case Format::NV12:
                return "NV12";
        }
        return "Unknown";
    }

    private:
    //根据格式调整数据缓冲区大小
    void resizeData() {
        switch (format) {
            case Format::YUV420:
                data.resize(width * height * 3 / 2);  // YUV420：Y占据宽×高，UV占据宽×高/4
                break;
            case Format::YUV422:
                data.resize(width * height * 2);  // YUV422：Y占据宽×高，UV占据宽×高/2
                break;
            case Format::NV12:
                data.resize(width * height * 3 / 2);  // NV12：Y占据宽×高，UV占据宽×高/2
                break;
            case Format::RGB24:
                data.resize(width * height * 3);  // RGB24：每个像素占3字节
                break;
            default:
                data.clear();
                break;
        }
    }
}

#endif // VIDEO_FRAME_H