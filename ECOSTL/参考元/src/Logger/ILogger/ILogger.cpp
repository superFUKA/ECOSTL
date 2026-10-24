#include "ILogger.h"
#include <iostream>
#include <assert.h>

namespace utl
{
	LoggerHolder::LoggerHolder(ILogger* logger)
		:logger_(logger)
	{
	}

	void LoggerHolder::Log(LogType level, const char* message, const char* file, int line)
	{
		if (logger_ == nullptr)
		{
			logger_->Log(level, message, file, line);
		}
		else
		{

			switch (level)
			{
			case utl::LogType::Debug:
				printf("Message\t:");
				printf(message);
				printf("\n");

				printf("File\t:");
				printf(file);
				printf("\n");

				printf("Line\t:");
				printf("%d", line);
				printf("\n");
				break;
			case utl::LogType::Warn:
				assert(0 && message);
				break;
			case utl::LogType::Error:
				assert(0 && message);
				break;
			default:
				break;
			}
		}
	}
}