#pragma once
#include <cinttypes>

namespace utl
{
	enum class LogType : uint8_t
	{
		Debug,
		Warn,
		Error
	};

	class ILogger
	{
	protected:
		friend class LoggerHolder;

		virtual ~ILogger() = default;

		virtual void Log(LogType level, const char* message, const char* file, int line) = 0;
	};

	class LoggerHolder
	{
	public:

		LoggerHolder(ILogger* logger = nullptr);

		void Log(LogType level, const char* message, const char* file, int line);

		ILogger* Get()const
		{
			return logger_;
		}

		void Reset(ILogger* logger = nullptr)
		{
			logger_ = logger;
		}

	private:

		ILogger* logger_;

	};
}

// デバッグログを残すマクロ
// プロジェクト設定のプリプロセッサに"UNENABLE_DEBUG"を追加したら無効化できる
#if defined(UNENABLE_DEBUG)
#define LOG_DEBUG(logger, message)
#else
#define LOG_DEBUG(logger, message)\
logger.Log(utl::LogType::Debug, message, __FILE__, __LINE__);
#endif

// 警告のログを残すマクロ
// プロジェクト設定のプリプロセッサに"UNENABLE_WARNING"を追加したら無効化できる
#if defined(UNENABLE_WARNING)
#define LOG_WARN(is,logger, message)
#else
#define LOG_WARN(is,logger, message)\
if(is) logger.Log(utl::LogType::Warn, message, __FILE__, __LINE__);
#endif

// エラーのログを残すマクロ
// プロジェクト設定のプリプロセッサに"UNENABLE_ERROR"を追加したら無効化できる
#if defined(UNENABLE_ERROR)
#define LOG_ERROR(is,logger, message)
#else
#define LOG_ERROR(is,logger, message)\
if(is) logger.Log(utl::LogType::Error, message, __FILE__, __LINE__);
#endif
