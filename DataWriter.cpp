#include "stdafx.h"
#include "DataWriter.h"

const std::string g_video_file_base("video_");
const std::string g_video_file_ext(".avi");
const std::string g_image_file_ext(".jpg");
const std::string g_metadata_file_ext(".txt");

DataWriter::DataWriter(const std::string& szTargetPath, const DataWriterType type, int fps) :
	m_szTargetPath(szTargetPath),
	m_type(type),
	m_bRunning(true),
	m_workerThread(&DataWriter::threadFunc, this),
	m_nFrameCounter(0)
{
	const int nFillSize = 4;

	if (!DirExists(szTargetPath))
	{
		CreateDirectory(s2ws(szTargetPath).c_str(), NULL);
	}

	time_t t = time(0);
	struct tm * now = localtime(&t);
	std::ostringstream ssDate;
	ssDate << (now->tm_year + 1900) << '_' << 
		std::setw(2) << std::setfill('0') << (now->tm_mon + 1) << '_' <<
		std::setw(2) << std::setfill('0') << now->tm_mday;

	std::ostringstream ssVideoPath;
	ssVideoPath << szTargetPath << '\\' << ssDate.str();

	if (!DirExists(ssVideoPath.str()))
	{
		CreateDirectory(s2ws(ssVideoPath.str()).c_str(), NULL);
	}

	std::deque<std::string> fileList = GetFileList(ssVideoPath.str(), g_video_file_base + "*" + g_video_file_ext);
	std::deque<std::string> dirList = GetDirList(ssVideoPath.str(), g_video_file_base + "*");

	int nLastVideoIndex = -1;

	if (fileList.size() > 0)
	{
		std::string szLastVideoName = fileList[fileList.size() - 1];
		std::string szLastVideoIndex = szLastVideoName.substr(szLastVideoName.size() - nFillSize - g_video_file_ext.size(), nFillSize);
		int index = std::stoi(szLastVideoIndex);
		if (index > nLastVideoIndex)
		{
			nLastVideoIndex = index;
		}
	}

	if (dirList.size() > 0)
	{
		std::string szLastVideoName = dirList[dirList.size() - 1];
		std::string szLastVideoIndex = szLastVideoName.substr(szLastVideoName.size() - nFillSize);
		int index = std::stoi(szLastVideoIndex);
		if (index > nLastVideoIndex)
		{
			nLastVideoIndex = index;
		}
	}

	const int nNextVideoIndex = nLastVideoIndex + 1;
	
	std::ostringstream ssFileNameBase;
	ssFileNameBase << ssVideoPath.str() << '\\' << g_video_file_base << std::setw(nFillSize) << std::setfill('0') << nNextVideoIndex;

	if (m_type == IMAGE_SEQUENCE_WRITER)
	{
		m_szVideoPath = ssFileNameBase.str();
		m_szImageFileBase = ssFileNameBase.str() + '\\';

		CreateDirectory(s2ws(m_szVideoPath).c_str(), NULL);
	}
	else if (m_type == VIDEO_WRITER)
	{
		m_szVideoFile = ssFileNameBase.str() + g_video_file_ext;

		// Codec: https://www.xvid.com (Xvid-1.3.4-20150621.exe)
		// Encoder config: "Other options..." -> uncheck "Display encoding status"
		int fourcc = CV_FOURCC('X', 'V', 'I', 'D');

		m_videoWriter.open(m_szVideoFile, fourcc, fps, cv::Size(cVideoWidth, cVideoHeight), true);

		if (!m_videoWriter.isOpened())
		{
			OutputDebugStringA("WARNING: Failed to open video writer.\n");
		}
	}

	m_szMetadataFile = ssFileNameBase.str() + g_metadata_file_ext;

	m_metadataWriter.open(m_szMetadataFile, std::ios::out);

	if (!m_metadataWriter.is_open())
	{
		OutputDebugStringA("WARNING: Failed to open text file.\n");
	}
}

DataWriter::~DataWriter()
{
	stopThread();
}

void DataWriter::appendFrame(FrameData& frameData)
{
	std::lock_guard<std::mutex> lock(m_bufferMutex);

	m_buffer.push_back(frameData);
}

unsigned long DataWriter::bufferSize()
{
	std::lock_guard<std::mutex> lock(m_bufferMutex);

	return m_buffer.size();
}

void DataWriter::threadFunc()
{
	OutputDebugStringA("Worker thread started.\n");

	while (m_bRunning)
	{
		FrameData data;

		m_bufferMutex.lock();

		if (!m_buffer.empty())
		{
			data = m_buffer.front();
			m_buffer.pop_front();
		}

		m_bufferMutex.unlock();

		if (data.frame.empty())
		{
			Sleep(0);
		}
		else
		{
			cv::resize(data.frame, data.frame, cv::Size(cVideoWidth, cVideoHeight));

			const int num_persons = data.persons.size();

			std::ostringstream metadata;
			metadata << num_persons << " ";

			for (int i = 0; i < num_persons; ++i)
			{
				PersonData person = data.persons[i];

				const int num_joints = person.joints.size();
				metadata << num_joints << " ";

				for (int j = 0; j < num_joints; ++j)
				{
					JointData joint = person.joints[j];
					metadata << joint.x << " " << joint.y << " " << joint.state << " ";
				}
			}

			if (m_type == IMAGE_SEQUENCE_WRITER)
			{
				std::ostringstream ssImageFileName;
				ssImageFileName << m_szImageFileBase << std::setw(6) << std::setfill('0') << m_nFrameCounter << g_image_file_ext;

				cv::imwrite(ssImageFileName.str(), data.frame);
			}
			else if (m_type == VIDEO_WRITER)
			{
				m_videoWriter << data.frame;
			}

			m_metadataWriter << metadata.str() << std::endl;

			m_bufferMutex.lock();

			++m_nFrameCounter;

			m_bufferMutex.unlock();
		}
	}

	m_bufferMutex.lock();
	unsigned long nBufferSize = m_buffer.size();
	m_bufferMutex.unlock();
		
	if (nBufferSize)
	{
		std::ostringstream ss;
		ss << "WARNING: " << nBufferSize << " frame(s) not written." << std::endl;

		OutputDebugStringA(ss.str().c_str());
	}
}

void DataWriter::stopThread()
{
	if (m_bRunning)
	{
		m_bRunning = false;
		m_workerThread.join();

		OutputDebugStringA("Worker thread stopped.\n");
	}
}

unsigned long DataWriter::frameCount()
{
	std::lock_guard<std::mutex> lock(m_bufferMutex);

	return m_nFrameCounter;
}

void DataWriter::eraseVideo()
{
	stopThread();
	
	m_videoWriter.release();
	m_metadataWriter.close();

	if (m_type == IMAGE_SEQUENCE_WRITER)
	{
		DeleteDir(m_szVideoPath);
	}
	else if (m_type == VIDEO_WRITER)
	{
		std::remove(m_szVideoFile.c_str());
	}

	std::remove(m_szMetadataFile.c_str());
}
