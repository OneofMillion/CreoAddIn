#include "stdafx.h"
#include "LogFileWriter.h"


LogFileWriter::LogFileWriter(const char* path= "D:\\LeoCreoAddin.log")
{
	//fopen_s(&logFile, path, "a"); // Append mode
}

LogFileWriter::~LogFileWriter()
{
	//fclose(logFile);
}

void LogFileWriter::WriteLog(const char* log){

	FILE* logFile;
	fopen_s(&logFile, "D:\\LeoCreoAddin.log", "a");
	if (logFile != NULL) {
		time_t now;
		time(&now);
		
		char timeStr[26];
		strncpy(timeStr, ctime(&now), 24); // Copy only the first 24 characters (excluding the newline)
		timeStr[24] = '\0'; // Ensure null termination
		fprintf(logFile, "[%s] %s\n\n", timeStr, log);
	}
	fclose(logFile);
}
