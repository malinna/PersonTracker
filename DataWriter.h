#pragma once

#include <d2d1.h>
#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <fstream>
#include <opencv2\opencv.hpp>

enum DataWriterType
{
	IMAGE_SEQUENCE_WRITER = 0, // Default
	VIDEO_WRITER = 1
};

struct JointData
{
	JointData(float px, float py, float s) : x(px), y(py), state(s) {}
	float x;
	float y;
	int state; // 0 = not tracked, 1 = inferred, 2 = tracked
};

struct PersonData
{
	inline void appendJoint(const JointData& j) { joints.push_back(j); }
	std::vector<JointData> joints;
};

struct FrameData
{
	inline void appendPerson(const PersonData& p) { persons.push_back(p); }
	cv::Mat	frame;
	std::vector<PersonData> persons;
};

class DataWriter
{
	static const int cVideoWidth = 1280;
	static const int cVideoHeight = 720;

public:
	DataWriter(const std::string& szTargetPath, const DataWriterType type, int fps=30);
	~DataWriter();

	void appendFrame(FrameData& frameData);

	unsigned long bufferSize();
	
	static int videoWidth()		{ return cVideoWidth; }
	static int videoHeight()	{ return cVideoHeight; }

	unsigned long frameCount();

	// Erase video and metadata files.
	// NOTE: This will stop the worker thread. The instance of this class should 
	// be deleted after calling this method.
	void eraseVideo(); 

private:
	void threadFunc();
	void stopThread();

private:
	std::string				m_szTargetPath;
	std::string				m_szVideoFile;
	std::string				m_szVideoPath;
	std::string				m_szImageFileBase;
	std::string				m_szMetadataFile;
	DataWriterType			m_type;
	std::deque<FrameData>	m_buffer;
	std::mutex				m_bufferMutex;
	std::atomic<bool>		m_bRunning;
	std::thread				m_workerThread;
	unsigned long			m_nFrameCounter;
	cv::VideoWriter			m_videoWriter;
	std::ofstream			m_metadataWriter;
};

