// ECOSTL のログ用マクロ（UTL_LOG_*）が ECOCppLogger につながり、書いたログを読み戻せることを確かめる。
#include <gtest/gtest.h>
#include "ECOSTL/Logger/Log.h"
#include "ECOSTL/Memory/Allocator/MallocAllocator.h"
#include "ECOSTL/Memory/Allocator/MemoryAllocator.h"
#include "ECOCppLogger/Reader.h"
#include <string>

namespace
{
	// readLog が渡す文字列を貯める
	void AppendText(const char* text, const size_t size, void* user)
	{
		static_cast<std::string*>(user)->append(text, size);
	}
}

TEST(Log, UtlLogReachesECOCppLogger)
{
	lg::Config config;
	config.directory = "ecostl-log-test";
	config.stem = "ECOSTLLogTest";
	ASSERT_TRUE(lg::start(config));
	const int value = 42;
	UTL_LOG_INFO("ECOSTL のログの確認 {value}", value);
	lg::writeOut();
	lg::stop();

	const char* path = lg::latestLog(config.directory);
	ASSERT_NE(path, nullptr);
	std::string text;
	ASSERT_TRUE(lg::readLog(path, lg::ViewFormat::Json, lg::ViewFilter{}, &AppendText, &text));
	EXPECT_NE(text.find("ECOSTL のログの確認"), std::string::npos);
	EXPECT_NE(text.find("42"), std::string::npos);
}

namespace
{
	// 必ず確保に失敗するアロケータ（失敗のログを確かめる）
	struct FailingAllocator
	{
		void* Allocate(uint32_t, uint32_t) { return nullptr; }
		bool Free(void*) { return false; }
	};
}

// MemoryAllocatorRef を通る確保・開放・失敗が、型 memory のログとして残る
TEST(Log, AllocatorWritesMemoryLog)
{
	lg::Config config;
	config.directory = "ecostl-log-test";
	config.stem = "ECOSTLAllocLogTest";
	ASSERT_TRUE(lg::start(config));

	utl::MallocAllocator baseAllocator;
	utl::MemoryAllocatorRef ref(baseAllocator.GetAPI());
	void* memory = ref.Allocate(48, 16);
	ASSERT_NE(memory, nullptr);
	EXPECT_TRUE(ref.Free(memory));

	FailingAllocator failing;
	utl::MemoryAllocatorDelegate failingAPI;
	failingAPI.Register<FailingAllocator, &FailingAllocator::Allocate, &FailingAllocator::Free>(&failing);
	utl::MemoryAllocatorRef failingRef(&failingAPI);
	EXPECT_EQ(failingRef.Allocate(1234, 0), nullptr);

	lg::writeOut();
	lg::stop();

	const char* path = lg::latestLog(config.directory);
	ASSERT_NE(path, nullptr);
	std::string text;
	ASSERT_TRUE(lg::readLog(path, lg::ViewFormat::Json, lg::ViewFilter{}, &AppendText, &text));
	EXPECT_NE(text.find("\"memory\""), std::string::npos);
	EXPECT_NE(text.find("確保 "), std::string::npos);
	EXPECT_NE(text.find("開放 "), std::string::npos);
	EXPECT_NE(text.find("確保に失敗"), std::string::npos);
	EXPECT_NE(text.find("1234"), std::string::npos);
}
