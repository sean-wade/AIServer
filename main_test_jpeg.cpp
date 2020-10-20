#include <opencv2/opencv.hpp>
#include "yolov3.hpp"
#include <fstream>
#ifdef USE_CURL
#include <curl/curl.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define Port 8847
#define BuffSize 1024

using namespace std;

bm_handle_t handle;
bm_device_mem_t dmem_bgr, dmem_resize, dmem_rgb;
double time_decode = 0;
double time_npu = 0;
double time_post_process = 0;

//比特大陆函数--不需要修改
int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame) {
	int frameFinished;
	int ret = -1;

	avcodec_decode_video2(pCodecContext, pFrame, &frameFinished, pPacket);
	if (frameFinished) {
		/* logging("Frame %d pts %d ", pCodecContext->frame_number, pFrame->pts); */
		bm_device_mem_t dmem_y = bm_mem_from_device((unsigned long long)pFrame->data[4],
				pFrame->linesize[4]*pFrame->height);
		bm_device_mem_t dmem_uv = bm_mem_from_device((unsigned long long)pFrame->data[5],
				pFrame->linesize[5]*pFrame->height/2);
		bm_yuv2bgr(handle, dmem_y, pFrame->linesize[4], dmem_uv, pFrame->linesize[5],
				pFrame->height, pFrame->width, 0, 0, dmem_bgr);

		bmdnn_img_scale_fit(handle, dmem_resize, dmem_bgr,
				1, 3,  // N=1, C=3 (B/G/R)
				992, pFrame->height, // dh/sh
				992, pFrame->width,  // dw/sw
				0, 0, 0, 0, 0,  // do not crop
				0, 128, 128, 128,
				4, 4, // data length
				0, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
		bm_bgrnorm(handle,dmem_resize ,992,992, 1.0f/256,0.0f,1.0f/256,0.0f,1.0f/256,0.0f,dmem_rgb );
		bm_bgr2rgb(handle, dmem_rgb,992,992, dmem_rgb);
		//av_frame_unref(pFrame);
		ret = 0;
	}
	return ret;
}

//比特大陆函数--可以修改结果字符串的格式
string show_frame(std::vector<det> dets_result, string img_string)
{
	string result_str = "data=[";
	cv::Mat frame = cv::imread(img_string);


	for(size_t k=0;k<dets_result.size();k++)
	{
		det det_k = dets_result[k];
		int left  = det_k.left;
		int right = det_k.right;
		int top   = det_k.top;
		int bot   = det_k.bot;
		int category= det_k.category;
		//cout << img_string << endl;
		//int red=255;
		//int green=255;
		//int blue=255;
		//if(category<9)
		//{
		//    red   = color_R[category];
		//    green = color_G[category];
		//    blue  = color_B[category];
		//}

		std::cout<< " !!! Found Target: " << category <<" "<<left<<" "<<right<<" "<<top<<" "<<bot<<" " << det_k.score << std::endl;

		//结果字符串赋值
		result_str += "{\"cls_idx\":\"";
		result_str += (to_string(category))+"\",";
		result_str += "\"coord\":\"";
		result_str += (to_string(left))+","+(to_string(top))+","+(to_string(right))+","+(to_string(bot))+"\"},";

		cv::rectangle(frame,cv::Point(left,top),cv::Point(right,bot),cv::Scalar(0,0,255),3,8,0);
	}

	result_str += "]";
	//cv::imwrite("result.jpg",frame);
	//cv::namedWindow("result",CV_WINDOW_NORMAL);
	//cv::setWindowProperty("result", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN );
	//cv::imshow("result", frame);
	//cvWaitKey(10);
	//   cout << result_str << endl;
	return result_str; 
}

//比特大陆函数--不需要修改
std::vector<det> detection_yolov3_process(detection *dets, int nboxes, int width, int height)
{

	int rows=height;
	int cols=width;
	cout << "width&&height: " << cols << " ---- " << rows << endl;
	int classes=2;
	//float nms=.45;
	float thresh=0.55;

	//int nboxes = 0;
	//image sized=image_convert(frame,w, h);
	std::vector<det> dets_all;

	for(int k=0;k<nboxes;k++)//做过nms之后的cell 包含一个框和类别信息
	{
		box b = dets[k].bbox;
		//        std::cout<<b.x<<" "<<b.y<<" "<<b.w<<" "<<b.y<<" "<<frame.cols<<" "<<frame.rows<<std::endl;
		int left  = (b.x-b.w/2.)*cols;
		int right = (b.x+b.w/2.)*cols;
		int top   = (b.y-b.h/2.)*rows;
		int bot   = (b.y+b.h/2.)*rows;



		if(left < 0) left = 0;
		if(right > cols-1) right = cols-1;
		if(top < 0) top = 0;
		if(bot > rows-1) bot = rows-1;
		//std::cout<<left<<" "<<right<<" "<<top<<" "<<bot<<std::endl;

		int category = max_index(dets[k].prob, classes);
		if(dets[k].prob[category] > thresh)
		{
			det det_k;
			det_k.left = left;
			det_k.right= right;
			det_k.top  = top;
			det_k.bot  = bot;
			det_k.category = category;
			det_k.score    = dets[k].prob[category];

			dets_all.push_back(det_k);

		}
	}

	return dets_all;
}



int main(int argc, char** argv)
{
	//初始化比特大陆相关计算资源
	double time_start=what_time_is_it_now();

	string context_dir = "./yolov3/yolov3_ir/";
	bm_dev_request(&handle, 0, 0);
	struct bmrt_info g_bmrtinfo;
	g_bmrtinfo.bm_handle = handle;
	bmruntime* p_bmrt = new bmruntime(handle);
	p_bmrt->load_context(context_dir);
	// fill bmrt net info
	bmrt_init_net_info(p_bmrt, g_bmrtinfo);
	bmrt_print_net_info(g_bmrtinfo);
	bmrt_alloc_device_memory(g_bmrtinfo);
	bmrt_net_info* pnetinfo = g_bmrtinfo.netinfo;
	bm_device_mem_t* input_mem  = pnetinfo->input_mem_ping;
	bm_device_mem_t* output_mem = pnetinfo->output_mem_ping;

	dmem_rgb = input_mem[0];

	int SRC_W, SRC_H;

	bm_malloc_device_byte(handle, &dmem_resize, (992*992*3*4));

	float* output_s_addr = new float[31 * 31 * 21];
	float* output_m_addr = new float[62 * 62 * 21];
	float* output_l_addr = new float[124 * 124 * 21];

	int count = 0;
	double time_decode_start = 0;
	double time_decode_stop = 0;
	double time_npu_stop = 0;
	double time_decode_detect=0;

#ifdef USE_CURL
	CURL* curl;
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.2.104:5000");
#endif

	//初始化socket
	/******** Program Variable Define & Initialize **********/
	int Main_Socket;    // Main Socket For Server
	int Communication_Socket; // Socket For Special Clients
	int Status; // Status Of Function
	struct sockaddr_in Server_Address; // Address Of Server
	struct sockaddr_in Client_Address;// Address Of Client That Communicate with Server
	char Buff[100] = "";
	printf ("Server Communicating By Using Port %d\n", Port);
	/******** Create A Socket To Communicate With Server **********/
	Main_Socket = socket ( AF_INET, SOCK_STREAM, 0 );
	if ( Main_Socket == -1 )
	{
		printf ("Sorry, System Can Not Create Socket!\n");
	}
	/******** Create A Address For Server To Communicate **********/
	Server_Address.sin_family = AF_INET;
	Server_Address.sin_port = htons(Port);
	Server_Address.sin_addr.s_addr = htonl(INADDR_ANY);
	/******** Bind Address To Socket **********/
	Status = bind ( Main_Socket, (struct sockaddr*)&Server_Address, sizeof(Server_Address) );
	if ( Status == -1 )
	{
		printf ("Sorry System Can Not Bind Address to The Socket!\n");
	}
	/******** Listen To The Port to Any Connection **********/        
	listen (Main_Socket,12);    
	socklen_t Lenght = sizeof (Client_Address);

	//定义接受图片的名字,保存在当前路径下
	string name = "detect.jpg";
    //循环接收客户端的识别请求
	while (1)
	{
		Communication_Socket = accept (Main_Socket, (struct sockaddr*)&Client_Address, &Lenght);

		FILE *fp=fopen(name.c_str(),"wb");
		int i = 0;
		bool result = true;
		unsigned char Buffer[BuffSize]={0};
		while(result)
		{
			int len = 2;
			int ilen =0;
			while(ilen < len) {
				int k = recv(Communication_Socket, Buffer+ilen, len-ilen, 0);
				if((k < 0) || (k ==0)) {
					printf("recv Faield!\n");
					result =false;
					break;
				}
				ilen += k;

			}
			len = Buffer[0] * 256 + Buffer[1]+2;
			while(ilen < len) {
				int k = recv(Communication_Socket, Buffer+ilen, len-ilen, 0);
				if(k < 0) {
					printf("recv Faield!\n");
					result =false;
					break;
				}
				else if(k ==0 ) {
					printf("recv Faield!\n");
					result =false;
					break;
				}
				ilen += k;
			}

			Buffer[len] = 0x00;
			//寻找到文件传输结束标志
			if((len == 34) && (strcmp((char *)Buffer+2,"FightForWuGaoSuo2019_YiJiaHe_XD_") == 0))
			{
				printf("Found End Sysbol\n");
				break;
			}
			else
			{
				fwrite(Buffer+2, 1, len-2, fp);
			}
		}
		fclose(fp);
        //完成文件写入,关闭描述符
		//object detection

		//执行异物检测计算
		AVDictionary* dict = NULL;
		double time_start1=what_time_is_it_now();
		av_dict_set(&dict, "chroma_interleave", "1", 0);
		/* The bitstream buffer is 3100KB,
		 * * assuming the max dimension is 1920x1080 */

		av_dict_set(&dict, "bs_buffer_size", "31000", 0);
		AVFormatContext *pFormatContext = avformat_alloc_context();
		//读入图片
		if (!pFormatContext || avformat_open_input(&pFormatContext, name.c_str(), NULL, NULL)!=0
				|| avformat_find_stream_info(pFormatContext,  NULL)<0) return -1;
		AVCodec *pCodec = NULL;
		AVCodecParameters *pCodecParameters =  NULL;
		int video_stream_index = -1;

		for (unsigned int i = 0; i < pFormatContext->nb_streams; i++) {
			AVCodecParameters *pLocalCodecParameters =  NULL;
			pLocalCodecParameters = pFormatContext->streams[i]->codecpar;
			if (pLocalCodecParameters->codec_id == AV_CODEC_ID_MJPEG) {
				pCodec = avcodec_find_decoder_by_name("jpeg_bm");
			} else {
				pCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);
			}
			if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
				video_stream_index = i;
				pCodecParameters = pFormatContext->streams[i]->codecpar;
				SRC_W = pLocalCodecParameters->width;
				SRC_H = pLocalCodecParameters->height;
				cout << "src_w: " << SRC_W << "src_h" << SRC_H << endl;
				logging("Video Codec: resolution %d x %d",
						pLocalCodecParameters->width,
						pLocalCodecParameters->height);
			};
		}

		bm_malloc_device_byte(handle, &dmem_bgr, (SRC_W*SRC_H*3*4));


		AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
		if (!pCodecContext || avcodec_parameters_to_context(pCodecContext, pCodecParameters)<0
				|| avcodec_open2(pCodecContext, pCodec, &dict)<0) return -1;
		AVPacket *pPacket = av_packet_alloc();
		if (!pPacket) return -1;
		AVFrame *pFrame = av_frame_alloc();
		if (!pFrame) return -1;

		double time_start1_end=what_time_is_it_now();

		printf("One frame init time is  %f s.\n", time_start1_end - time_start1);

		av_read_frame(pFormatContext, pPacket);
		string detect_result = "Error"; 
		if(pPacket->stream_index == video_stream_index)
		{
			time_decode_start = what_time_is_it_now();
			if(decode_packet(pPacket, pCodecContext, pFrame)<0){
				cout<<"load img error";
				// continue;
				return -1;
			};
			time_decode_stop = what_time_is_it_now();
			time_decode += (time_decode_stop - time_decode_start);
			count ++;

			// launch network do inference
			//float * input_data = new float[3*992*992];
			//bm_memcpy_d2s(g_bmrtinfo.bm_handle, bm_mem_from_system(input_data), input_mem[0]);
			p_bmrt->launch(0, input_mem, pnetinfo->itensor_num,
					output_mem, pnetinfo->otensor_num, pnetinfo->pitensor_info[0].N,
					pnetinfo->input_height[0], pnetinfo->input_width[0]);

			bm_memcpy_d2s(g_bmrtinfo.bm_handle, bm_mem_from_system(output_s_addr), output_mem[1]);
			bmdnn_sigmoid_forward(g_bmrtinfo.bm_handle, output_mem[1], 1, 31 * 31 * 21, output_mem[1]);
			bmdnn_sigmoid_forward(g_bmrtinfo.bm_handle, output_mem[2], 1, 62 * 62 * 21, output_mem[2]);
			bmdnn_sigmoid_forward(g_bmrtinfo.bm_handle, output_mem[0], 1, 124 * 124 * 21, output_mem[0]);

			bm_memcpy_d2s(g_bmrtinfo.bm_handle, bm_mem_from_system(output_s_addr), output_mem[1]);
			bm_memcpy_d2s(g_bmrtinfo.bm_handle, bm_mem_from_system(output_m_addr), output_mem[2]);
			bm_memcpy_d2s(g_bmrtinfo.bm_handle, bm_mem_from_system(output_l_addr), output_mem[0]);

			vector<float*> blobs;
			blobs.push_back(output_s_addr);
			blobs.push_back(output_m_addr);
			blobs.push_back(output_l_addr);

			bmrt_return(blobs);
			time_npu_stop = what_time_is_it_now();
			time_npu += (time_npu_stop - time_decode_stop);

			int nboxes = 0;
			cout << "w: " << SRC_W << " " << "h: " << SRC_H << endl;

			detection* dets = get_detections(blobs, SRC_W, SRC_H, &nboxes);
			cout<<"get_detections end"<<endl;

			std::vector<det> det_results=detection_yolov3_process(dets, nboxes, SRC_W, SRC_H);

			cout<<"detection_yolov3_process end"<<endl;

			time_decode_detect=what_time_is_it_now()-time_decode_start;
			printf("time: %f\n", time_decode_detect);
			detect_result = show_frame(det_results, name);
			time_post_process += (what_time_is_it_now() - time_npu_stop);
#ifdef USE_CURL
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, detect_result.c_str());
			curl_easy_perform(curl);
#endif
		}

		av_free_packet(pPacket);
		av_free(pFrame);
		avcodec_close(pCodecContext);
		avformat_close_input(&pFormatContext);
		avformat_free_context(pFormatContext);

		printf("All decode+vpp time is  %f s.\n", time_decode);
		printf("All npu time is %f s, frame is %d.\n", time_npu, count);
		printf("All post process time is %f s.\n", time_post_process);

		double time_end1=what_time_is_it_now();
		printf("One frame time is  %f s.\n", time_end1 - time_start1);

		//返回结果字符串
		string resultStr = detect_result;
		int len = resultStr.length();
		Buffer[0] = len / 256;
		Buffer[1] = len % 256;
		memcpy(Buffer+2, resultStr.c_str(), len);
		len += 2;

		int ilen =0;
		while(ilen < len) {
			int k = send(Communication_Socket, Buffer+ilen , len-ilen, 0);
			if(k < 0) {
				printf("send failed!\n");
				break;
			}
			ilen += k;
		}
		printf("ACK Send\n");
	}

	bm_free_device(handle, dmem_bgr);
	//  av_packet_unref(pPacket);

#ifdef USE_CURL
	curl_easy_cleanup(curl);
	curl_global_cleanup();
#endif

	// free memory and info
	bm_free_device(handle,dmem_resize);
	bmrt_free_device_memory(g_bmrtinfo);
	bmrt_delete_net_info(g_bmrtinfo);
	delete [] output_s_addr;
	delete [] output_l_addr;
	delete [] output_m_addr;
	delete p_bmrt;
	bm_dev_free(handle);

	return 0;
}
