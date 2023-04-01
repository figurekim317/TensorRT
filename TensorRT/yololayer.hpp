#pragma once
#include <common.h>
#include <fstream>

struct Yololayer {
	int C;
	int H;
	int W;
	int CLASS_NUM;
	int Grid_stride;
};

namespace nvinfer1
{
	class YololayerPluginV2 : public IPluginV2IOExt
	{
	public:
		YololayerPluginV2(const Yololayer& arg)
		{
			mYololayer = arg;
		}

		YololayerPluginV2(const void* data, size_t length)
		{
			const char* d = static_cast<const char*>(data);
			const char* const a = d;
			mYololayer = read<Yololayer>(d);
			assert(d == a + length);
		}
		YololayerPluginV2() = delete;

		virtual ~YololayerPluginV2() {}

	public:
		int getNbOutputs() const noexcept override
		{
			return 1;
		}

		Dims getOutputDimensions(int index, const Dims* inputs, int nbInputDims) noexcept override
		{
			return Dims2(mYololayer.H * mYololayer.W * mYololayer.C, 6); // ��� Tensor�� dimenson shape
		}

		int initialize() noexcept override
		{
			return 0;
		}

		void terminate() noexcept override
		{
		}

		// ���� enqueue �Լ����� �߰��� ����� ������ �ʿ� �ϴٸ� �ʿ��� ������ ũ�⸦ �����ϰ� ����
		// enqueue �Լ����� workspace �����͸� �̿��Ͽ� �� ���� ����
		size_t getWorkspaceSize(int maxBatchSize) const noexcept override
		{
			return 0;
		}

		// plugin�� ����� �����ϴ� �Լ�(���� �ʿ�)
		int enqueue(int batchSize, const void* const* inputs, void* const* outputs, void* workspace, cudaStream_t stream) noexcept override
		{
			float* input = (float*)inputs[0];
			float* anchor_grid = (float*)inputs[1];
			float* output = (float*)outputs[0];

			int Height = mYololayer.H;
			int Width = mYololayer.W;
			int CLASS_NUM = mYololayer.CLASS_NUM;
			int Grid_stride = mYololayer.Grid_stride;

			void yololayer_cu(float* output, float* input, float* anchor_grid, int batchSize, int height, int width, int CLASS_NUM, int Grid_stride, cudaStream_t stream);
			yololayer_cu(output, input, anchor_grid, batchSize, Height, Width, CLASS_NUM, Grid_stride, stream);
			
			// ��� ����
			//cudaDeviceSynchronize();
			//int count = batchSize * H * W * C;
			//std::cout << "count : " << count << std::endl;
			//std::vector<float> gpuBuffer(count);
			//cudaMemcpy(gpuBuffer.data(), output, gpuBuffer.size() * sizeof(float), cudaMemcpyDeviceToHost);
			//std::ofstream ofs("../Validation_py/trt_0", std::ios::binary);
			//if (ofs.is_open())
			//	ofs.write((const char*)gpuBuffer.data(), gpuBuffer.size() * sizeof(float));
			//ofs.close();
			//std::exit(0);

			// �Է� ����
			//cudaDeviceSynchronize();
			//int count = batchSize * H * W * C;
			//std::cout << "count : " << count << std::endl;
			//std::vector<uint8_t> gpuBuffer(count);
			//cudaMemcpy(gpuBuffer.data(), input, gpuBuffer.size() * sizeof(uint8_t), cudaMemcpyDeviceToHost);
			//std::ofstream ofs("../Validation_py/trt_1", std::ios::binary);
			//if (ofs.is_open())
			//	ofs.write((const char*)gpuBuffer.data(), gpuBuffer.size() * sizeof(uint8_t));
			//ofs.close();
			//std::exit(0);

			return 0;
		}

		size_t getSerializationSize() const noexcept override
		{
			size_t serializationSize = 0;
			serializationSize += sizeof(mYololayer);
			return serializationSize;
		}

		void serialize(void* buffer) const noexcept override
		{
			char* d = static_cast<char*>(buffer);
			const char* const a = d;
			write(d, mYololayer);
			assert(d == a + getSerializationSize());
		}

		void configurePlugin(const PluginTensorDesc* in, int nbInput, const PluginTensorDesc* out, int nbOutput) noexcept override
		{
		}

		//! The combination of kLINEAR + kINT8/kHALF/kFLOAT is supported.
		bool supportsFormatCombination(int pos, const PluginTensorDesc* inOut, int nbInputs, int nbOutputs) const noexcept override
		{
			assert(nbInputs == 2 && nbOutputs == 1 && pos < nbInputs + nbOutputs);
			bool condition = inOut[pos].format == TensorFormat::kLINEAR;
			condition &= inOut[pos].type != DataType::kINT32;
			condition &= inOut[pos].type == inOut[0].type;
			return condition;
		}
		// ����� ������ Ÿ�� ����
		DataType getOutputDataType(int index, const DataType* inputTypes, int nbInputs) const noexcept override
		{
			assert(inputTypes && nbInputs == 2);
			return DataType::kFLOAT; //
		}

		// plugin �̸� ���� 
		const char* getPluginType() const noexcept override
		{
			return "yololayer";
		}

		// �ش� plugin ���� �ο�
		const char* getPluginVersion() const noexcept override
		{
			return "1";
		}

		void destroy() noexcept override
		{
			delete this;
		}

		IPluginV2Ext* clone() const noexcept override
		{
			YololayerPluginV2* plugin = new YololayerPluginV2(*this);
			return plugin;
		}

		void setPluginNamespace(const char* libNamespace) noexcept override
		{
			mNamespace = libNamespace;
		}

		const char* getPluginNamespace() const noexcept override
		{
			return mNamespace.data();
		}

		bool isOutputBroadcastAcrossBatch(int outputIndex, const bool* inputIsBroadcasted, int nbInputs) const noexcept override
		{
			return false;
		}

		bool canBroadcastInputAcrossBatch(int inputIndex) const noexcept override
		{
			return false;
		}

	private:
		template <typename T>
		void write(char*& buffer, const T& val) const
		{
			*reinterpret_cast<T*>(buffer) = val;
			buffer += sizeof(T);
		}

		template <typename T>
		T read(const char*& buffer) const
		{
			T val = *reinterpret_cast<const T*>(buffer);
			buffer += sizeof(T);
			return val;
		}

	private:
		Yololayer mYololayer;
		std::string mNamespace;
	};

	class YololayerPluginV2Creator : public IPluginCreator
	{
	public:
		const char* getPluginName() const noexcept override
		{
			return "yololayer";
		}

		const char* getPluginVersion() const noexcept override
		{
			return "1";
		}

		const PluginFieldCollection* getFieldNames() noexcept override
		{
			return nullptr;
		}

		IPluginV2* createPlugin(const char* name, const PluginFieldCollection* fc) noexcept override
		{
			YololayerPluginV2* plugin = new YololayerPluginV2(*(Yololayer*)fc);
			mPluginName = name;
			return plugin;
		}

		IPluginV2* deserializePlugin(const char* name, const void* serialData, size_t serialLength) noexcept override
		{
			auto plugin = new YololayerPluginV2(serialData, serialLength);
			mPluginName = name;
			return plugin;
		}

		void setPluginNamespace(const char* libNamespace) noexcept override
		{
			mNamespace = libNamespace;
		}

		const char* getPluginNamespace() const noexcept override
		{
			return mNamespace.c_str();
		}

	private:
		std::string mNamespace;
		std::string mPluginName;
	};
	REGISTER_TENSORRT_PLUGIN(YololayerPluginV2Creator);
};
