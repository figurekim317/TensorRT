// 2021-8-19 by YH PARK 
// custom plugin
// preprocess(NHWC->NCHW, BGR->RGB, [0, 255]->[0, 1](Normalize))
#include "utils.hpp"		// custom function
#include "preprocess.hpp"	// preprocess plugin 
#include "logging.hpp"	

using namespace nvinfer1;
sample::Logger gLogger;

void main() 
{
	std::cout << "===== custom plugin example start =====" << std::endl;
	char engineFileName[] = "../Engine/plugin_test.engine";

	// 0. �̹������� ���� ��� �ҷ�����
	std::string img_dir = "../TestDate/";
	std::vector<std::string> file_names;
	if (SearchFile(img_dir.c_str(), file_names) < 0) {
		std::cerr << "Data search error" << std::endl;
	}
	else {
		std::cout << "Total number of images : "<< file_names.size() << std::endl;
	}

	// 1. �̹��� ������ �ε�
	int batch_size{ 1 };
	int input_width{ 224 };
	int input_height{ 224 };
	int input_channel{ 3 };
	const char* INPUT_NAME = "inputs";
	const char* OUTPUT_NAME = "outputs";
	cv::Mat img(input_height, input_width, CV_8UC3);
	cv::Mat ori_img;
	std::vector<uint8_t> input(batch_size * input_height * input_width * input_channel);	// �Է��� ��� �����̳� ���� ����
	std::vector<float> output(batch_size* input_channel * input_height * input_width);		// ����� ��� �����̳� ���� ����
	
	for (int idx = 0; idx < file_names.size(); idx++) {
		cv::Mat ori_img = cv::imread(file_names[idx]);
		//cv::resize(ori_img, img, img.size()); // input size�� ��������
		memcpy(input.data(), ori_img.data, batch_size * input_height * input_width * input_channel * sizeof(uint8_t));
	}
	std::cout << "===== input load done =====" << std::endl;

	//==========================================================================================

	std::cout << "===== Create TensorRT Model =====" << std::endl; // TensorRT �� ����� ����
	IBuilder* builder = createInferBuilder(gLogger);
	IBuilderConfig* config = builder->createBuilderConfig();
	unsigned int maxBatchSize = 1;	// ������ TensorRT �������Ͽ��� ����� ��ġ ������ �� 
	
	// ��Ʈ��ũ ������ ����� ���� ��Ʈ��ũ ��ü ����
	INetworkDefinition* network = builder->createNetworkV2(0U);
	
	// �Է�(Input) ���̾� ����
	ITensor* input_tensor = network->addInput(INPUT_NAME, nvinfer1::DataType::kFLOAT, Dims3{input_height, input_width, input_channel }); // [N,C,H,W]
	
	// Custom(preprocess) plugin ����ϱ�
	// Custom(preprocess) plugin���� ����� ����ü ��ü ����
	Preprocess preprocess{batch_size, input_channel, input_height, input_width, 0};
	// Custom(preprocess) plugin�� global registry�� ��� �� plugin Creator ��ü ����
	IPluginCreator* preprocess_creator = getPluginRegistry()->getPluginCreator("preprocess", "1");
	// Custom(preprocess) plugin ����
	IPluginV2 *preprocess_plugin = preprocess_creator->createPlugin("preprocess_plugin", (PluginFieldCollection*)&preprocess);
	// network ��ü�� custom(preprocess) plugin�� ����Ͽ� custom(preprocess) ���̾� �߰�
	IPluginV2Layer* preprocess_layer = network->addPluginV2(&input_tensor, 1, *preprocess_plugin);
	preprocess_layer->setName("preprocess_layer"); // layer �̸� ����
	preprocess_layer->getOutput(0)->setName(OUTPUT_NAME);// ��°� Tensor �̸��� ��� �̸����� ���� 
	network->markOutput(*preprocess_layer->getOutput(0));// preprocess_layer�� ��°��� �� Output���� ����

	builder->setMaxBatchSize(maxBatchSize); // ���� ��ġ ������ ����
	config->setMaxWorkspaceSize(1ULL << 26); // 64MB, ���� ������ ���� ����� �޸� ���� ����

	std::cout << "Building engine, please wait for a while..." << std::endl;
	IHostMemory* engine0 = builder->buildSerializedNetwork(*network, *config); // ���� ����(����)
	std::cout << "==== model build done ====" << std::endl << std::endl;

	std::cout << "==== model selialize start ====" << std::endl << std::endl;
	std::ofstream p(engineFileName, std::ios::binary);
	if (!p) {
		std::cerr << "could not open plan output file" << std::endl;
	}
	p.write(reinterpret_cast<const char*>(engine0->data()), engine0->size());
	std::cout << "==== model selialize done ====" << std::endl << std::endl;
	builder->destroy();
	config->destroy();
	engine0->destroy();
	network->destroy();
	p.close();
	//==========================================================================================

	std::cout << "===== Engine file deserialize =====" << std::endl << std::endl;
	char *trtModelStream{ nullptr };// ����� ��Ʈ���� ������ ����
	size_t size{ 0 };
	std::cout << "===== Engine file load =====" << std::endl << std::endl;
	std::ifstream file(engineFileName, std::ios::binary);
	if (file.good()) {
		file.seekg(0, file.end);
		size = file.tellg();
		file.seekg(0, file.beg);
		trtModelStream = new char[size];
		file.read(trtModelStream, size);
		file.close();
	}
	else {
		std::cout << "[ERROR] Engine file load error" << std::endl;
	}
	std::cout << "===== Engine file deserialize =====" << std::endl << std::endl;

	IRuntime* runtime = createInferRuntime(gLogger);
	ICudaEngine* engine = runtime->deserializeCudaEngine(trtModelStream, size);
	IExecutionContext* context = engine->createExecutionContext();
	delete[] trtModelStream;
	void* buffers[2]; // �Է°��� ��°��� �ְ� �ޱ� ���� ������ ���� ���� 

	// ��Ʈ��ũ �����Ҷ� ����� �Է°� ����� �̸����� �ε����� �޾� ���� 
	const int inputIndex = engine->getBindingIndex(INPUT_NAME);
	const int outputIndex = engine->getBindingIndex(OUTPUT_NAME);

	// GPU�� ���� ����(device�� ���� ���� �Ҵ�)
	CHECK(cudaMalloc(&buffers[inputIndex], maxBatchSize * input_channel * input_height * input_width * sizeof(uint8_t)));
	CHECK(cudaMalloc(&buffers[outputIndex], maxBatchSize * input_channel * input_height * input_width * sizeof(float)));

	// Cuda ��Ʈ�� ��ü ����
	cudaStream_t stream;
	CHECK(cudaStreamCreate(&stream));
	// GPU�� �Է� ������ ���� (CPU -> GPU)
	CHECK(cudaMemcpyAsync((uint8_t*)buffers[inputIndex], (char*)input.data(), maxBatchSize * input_channel * input_height * input_width * sizeof(uint8_t), cudaMemcpyHostToDevice, stream));
	// ��ġ������ �񵿱�� �۾� ���� 
	context->enqueue(maxBatchSize, buffers, stream, nullptr);
	// CPU�� ��� ������ �������� (CPU <- GPU)
	CHECK(cudaMemcpyAsync(output.data(), buffers[outputIndex], maxBatchSize * input_channel * input_height * input_width * sizeof(float), cudaMemcpyDeviceToHost, stream));
	// ��Ʈ�� ������ ����ȭ ����
	cudaStreamSynchronize(stream);
	std::cout << "===== TensorRT Model Calculate done =====" << std::endl;
	//==========================================================================================

	tofile(output, "../Validation_py/C_Preproc_Result"); // ����� ���Ϸ� ���
	//../Validation_py/valide_preproc.py ���� ��� �� ����

	// �ڿ� ���� �۾�
	cudaStreamDestroy(stream);
	CHECK(cudaFree(buffers[inputIndex]));
	CHECK(cudaFree(buffers[outputIndex]));
	context->destroy();
	engine->destroy();
	runtime->destroy();
}