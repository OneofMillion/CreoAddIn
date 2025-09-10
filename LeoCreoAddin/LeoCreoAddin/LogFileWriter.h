#pragma once

#include <string>

class LogFileWriter
{
private:
	//static FILE* logFile;

public:
	LogFileWriter(const char*);
	~LogFileWriter();
	static void WriteLog(const char*);
};

