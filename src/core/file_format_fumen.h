#pragma once

#include "core_types.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace Fumen
{
    class FumenParseException : public std::runtime_error
    {
    public:
        FumenParseException(const std::string &reason, size_t offset)
            : std::runtime_error(FormatMessage(reason, offset)), m_Reason(reason), m_Offset(offset)
        {
        }

        const std::string &GetReason() const { return m_Reason; }
        size_t GetOffset() const { return m_Offset; }

    private:
        std::string m_Reason;
        size_t m_Offset;

        static std::string FormatMessage(const std::string &reason, size_t offset)
        {
            return "Fumen parse error at offset 0x" + ToHexString(offset) + " (" + std::to_string(offset) + " bytes): " + reason;
        }

        static std::string ToHexString(size_t value)
        {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%zX", value);
            return buffer;
        }
    };

    // 写入Fumen文件时抛出的异常
    class FumenWriteException : public std::runtime_error
    {
    public:
        FumenWriteException(const std::string &reason, const std::string &filePath = "")
            : std::runtime_error(FormatMessage(reason, filePath)), m_Reason(reason), m_FilePath(filePath)
        {
        }

        const std::string &GetReason() const { return m_Reason; }
        const std::string &GetFilePath() const { return m_FilePath; }

    private:
        std::string m_Reason;
        std::string m_FilePath;

        static std::string FormatMessage(const std::string &reason, const std::string &filePath)
        {
            if (filePath.empty())
                return "Fumen write error: " + reason;
            else
                return "Fumen write error (" + filePath + "): " + reason;
        }
    };
    static constexpr std::string_view Extension = ".fumen";
    static constexpr std::string_view PreimageExtensions = ".fumen;.bin;";
    static constexpr std::string_view FilterName = "Taiko Nijiiro FumenV2 Chart";
    static constexpr std::string_view FilterSpec = "fumen";

    enum ClearHPDefault : u32
    {
        ClearHP_Easy = 6000,
        ClearHP_Normal_Hard = 7000,
        ClearHP_Oni = 8000
    };

    // Format documentation based on this source:
    // https://discord.com/channels/859872100860035113/859878630422609972/859954716511436850

    namespace FormatV2
    {

#pragma pack(push, 1)

        // Difficulty levels used in Fumen charts.
        // The value also corresponds to the suffix character used in the filename with underscore.
        // For example: "natsu_e.bin" is an Easy chart of song with id "natsu".
        enum class Difficulty : u8
        {
            Easy = 'e',
            Normal = 'n',
            Hard = 'h',
            Oni = 'm',
            Ura = 'x'
        };

        struct JudgeTimingBlock
        {
            f32 GoodTiming;
            f32 OkTiming;
            f32 BadTiming;

            void ResetJudgeTiming(Difficulty difficulty);
            void ResetJudgeTiming(f32 goodTiming, f32 okTiming, f32 badTiming);
        };

        struct Header
        {
            JudgeTimingBlock JudgeTimings[36];
            // whether the chart has divergent paths. 1 if yes, 0 if no. (set to 01000001 if you'll be switching endianness)
            u32 HasDivergentPaths;
            // Default is 10000
            u32 MaxHP;
            // Clear HP. _e is 6000, _n & _h is 7000 and _m is 8000.
            u32 ClearHP;
            // HP earned per 良 (Good)
            i32 HPPerGood;
            // HP earned per 可 (Ok)
            i32 HPPerOk;
            // HP lost per 不可 (Bad), negative value.
            i32 HPPerBad;
            // 普通 max combo/普通 max combo. Usually set to 65536.
            u32 MaxCombo;
            // Ratio of hp increase. 65536(普通 max combo/玄人 max combo). If no branch paths set to 65536.
            u32 HPIncreaseRatio;
            // Ratio of hp increase. 65536(普通 max combo/達人 max combo). If no branch paths set to 65536.
            u32 HPIncreaseRatioMaster;
            // 良 diverge points.	Common value is 20.
            u32 GoodDivergePoints;
            // 可 diverge points.	Common value is 10.
            u32 OkDivergePoints;
            // 不可 diverge points.	Common value is 0.
            u32 BadDivergePoints;
            // 連打　diverge points.	Common value is 1.
            u32 DrumrollDivergePoints;
            // 良 diverge points (big notes).	Common value is 20.
            u32 GoodDivergePointsBig;
            // 可 diverge points (big notes).	Common value is 10.
            u32 OkDivergePointsBig;
            // 連打（大）　diverge points.	Common value is 1.
            u32 DrumrollDivergePointsBig;
            // Balloon diverge points.	Common value is 30.
            u32 BalloonDivergePoints;
            // Bell/Yam diverge points.	Common value is 30.
            u32 BellYamDivergePoints;
            // Number of diverge points??	Common value is 20.
            u32 NumberOfDivergePoints;
            // Max Score value (deprecated). Dummy data.
            u32 MaxScoreValue;
            // Number of measures. The limit is 300, 200 on older platforms.
            u32 NumberOfMeasures;
            // Unknown data.
            u32 _Unknown1;
            // Then following data is repeated for each measure.

            void ResetJudgeTiming(Difficulty difficulty);
            void ResetJudgeTiming(f32 goodTiming, f32 okTiming, f32 badTiming);
        };

        struct MeasureData
        {
            // BPM (float)
            f32 BPM;
            // Offset (float). Formula for calculation: (240000/bpm1)+(240000/bpm1)(x/y)-(240000/bpm2)
            f32 MeasureOffset;
            // Gogo time. 1 if yes, 0 if no.
            u8 IsGogoTime;
            // Barline visibility.  1 if yes, 0 if no.
            u8 IsBarLineVisible;
            // Dummy data.
            u16 _Padding1;
            // 普通譜面. 普通-玄人 diverge point requirement.
            // For charts without divergent paths set to FFFFFFFF.
            u32 InNormalToAdvancedDivergePointReq;
            // 普通譜面. 普通-達人 diverge point requirement.
            // For charts without divergent paths set to FFFFFFFF.
            u32 InNormalToMasterDivergePointReq;
            // 玄人譜面. 玄人-玄人 diverge point requirement.
            // For charts without divergent paths set to FFFFFFFF.
            u32 InAdvancedToMasterDivergePointReq;
            // 玄人譜面. 玄人-達人 diverge point requirement.
            // For charts without divergent paths set to FFFFFFFF.
            u32 InAdvancedKeepAdvancedDivergePointReq;
            // 達人譜面. 達人-玄人 diverge point requirement.
            // For charts without divergent paths set to FFFFFFFF.
            u32 InMasterToAdvancedDivergePointReq;
            // 達人譜面. 達人-達人 diverge point requirement.
            // For charts without divergent paths set to FFFFFFFF.
            u32 InMasterKeepMasterDivergePointReq;
            // Dummy data.
            u32 _Padding2;
            // Then following data is repeated for each branch in the measure.
            // - Note data (普通譜面)
            // - Note data (玄人譜面)
            // - Note data (達人譜面)
        };

        struct MeasureNotesData
        {
            // Number of notes in this branch of the measure.
            u16 NumberOfNotes;
            // Dummy data.
            u16 _Padding1;
            // Scroll speed (float). Default is 1.
            f32 ScrollSpeed;
            // Then following data is repeated for notes.
        };

        static_assert(sizeof(MeasureNotesData) == 8, "NoteData size mismatch");

        enum NoteType : u32
        {
            NoteType_None = 0,
            NoteType_Don = 1,
            NoteType_Do = 2,
            NoteType_Ko = 3,
            NoteType_Katsu = 4,
            NoteType_Ka = 5,
            NoteType_Renda = 6,
            NoteType_BigDon = 7,
            NoteType_BigKatsu = 8,
            NoteType_BigRenda = 9,
            NoteType_Balloon = 10,
            NoteType_Bell = 12
        };

        struct NoteData
        {
            // The note type. Check NoteType enum.
            NoteType Type;
            // Offset within the measure in milliseconds.
            f32 NoteOffset;
            // Dummy data.
            u32 _Padding1;
            // Initial score value.
            // If the note is a balloon then it's the balloon hit count.
            u16 InitialScoreValue;
            // Score difference *4.
            // If the note is a balloon then it's zero.
            u16 ScoreDifferenceTimes4;
            // Dummy Data. Old balloon count.
            u32 BalloonHitCount_Old;
            // Length of 連打 and balloons (float). Zero otherwise.
            f32 Length;

            // Check whether the note is a renda(or big renda) note.
            // Is a note is a renda(big renda) note, then the note data will have 8 bytes of padding (or unknown data?) after it.
            bool isRendaNote() const
            {
                return Type == NoteType::NoteType_Renda || Type == NoteType::NoteType_BigRenda;
            }
        };

        static_assert(sizeof(NoteData) == 24, "NoteData size mismatch");

#pragma pack(pop)

        // Branch path index
        enum class BranchPath : u8
        {
            Normal = 0,   // 普通譜面
            Advanced = 1, // 玄人譜面
            Master = 2,   // 達人譜面
            Count = 3
        };

        // Represents a complete measure with all three branch paths
        struct Measure
        {
            MeasureData Data;
            f32 NormalNotesScrollSpeed = 1.0f;
            f32 AdvancedNotesScrollSpeed = 1.0f;
            f32 MasterNotesScrollSpeed = 1.0f;
            std::vector<NoteData> NormalNotes;   // 普通譜面
            std::vector<NoteData> AdvancedNotes; // 玄人譜面
            std::vector<NoteData> MasterNotes;   // 達人譜面

            Measure() = default;
        };

        // High-level representation of a Fumen chart
        class FumenChart
        {
        public:
            Header ChartHeader;
            std::vector<Measure> Measures;

            FumenChart() = default;

            // Get the number of measures in the chart
            size_t GetMeasureCount() const { return Measures.size(); }

            // Get the difficulty from header if available
            Difficulty GetDifficulty() const;

            // Add a new measure to the chart
            void AddMeasure(const Measure &measure);

            // Clear all data
            void Clear();

            // Check if chart has divergent paths
            bool HasDivergentPaths() const { return ChartHeader.HasDivergentPaths != 0; }
        };

        // Reader for Fumen FormatV2 binary files
        class FumenChartReader
        {
        public:
            FumenChartReader() = default;

            // Read a fumen chart from a file path
            // Throws FumenParseException on parse errors, std::runtime_error on file I/O errors
            void ReadFromFile(const std::string &filePath, FumenChart &outChart);

            // Read a fumen chart from a memory buffer
            // Throws FumenParseException on parse errors
            void ReadFromMemory(const u8 *data, size_t dataSize, FumenChart &outChart);

        private:
            const u8 *m_DataStart = nullptr;
            size_t m_CurrentMeasureIndex = 0;

            void ReadHeader(const u8 *&data, const u8 *dataEnd, Header &outHeader);
            void ReadMeasure(const u8 *&data, const u8 *dataEnd, Measure &outMeasure);
            void ReadMeasureNotes(const u8 *&data, const u8 *dataEnd, std::vector<NoteData> &outNotes, f32 &scrollSpeed);

            [[noreturn]] void ThrowError(const std::string &reason, const u8 *position) const
            {
                size_t offset = position ? (position - m_DataStart) : 0;
                throw FumenParseException(reason, offset);
            }

            template <typename T>
            void ReadData(const u8 *&data, const u8 *dataEnd, T &outValue, bool peek = false);
        };

        // Writer for Fumen FormatV2 binary files
        class FumenChartWriter
        {
        public:
            FumenChartWriter() = default;

            // Write a fumen chart to a file path
            // Throws FumenWriteException on write errors
            void WriteToFile(const std::string &filePath, const FumenChart &chart);

            // Write a fumen chart to a memory buffer (returns the buffer)
            std::vector<u8> WriteToMemory(const FumenChart &chart);

        private:
            void WriteHeader(std::vector<u8> &buffer, const Header &header);
            void WriteMeasure(std::vector<u8> &buffer, const Measure &measure);
            void WriteMeasureNotes(std::vector<u8> &buffer, const std::vector<NoteData> &notes, f32 scrollSpeed);

            template <typename T>
            void WriteData(std::vector<u8> &buffer, const T &value);
        };
    }

}
