#include "file_format_fumen.h"
#include <fstream>
#include <cstring>

namespace Fumen::FormatV2
{
    void JudgeTimingBlock::ResetJudgeTiming(Difficulty difficulty)
    {
        switch (difficulty)
        {
        case Difficulty::Easy:
        case Difficulty::Normal:
            ResetJudgeTiming(41.70833588f, 108.4416656f, 125.125f);
            break;
        case Difficulty::Hard:
        case Difficulty::Oni:
        case Difficulty::Ura:
            ResetJudgeTiming(25.0250015f, 75.0750046f, 108.441666f);
            break;
        default:
            // Do nothing for other difficulties.
            break;
        }
    }

    void JudgeTimingBlock::ResetJudgeTiming(f32 goodTiming, f32 okTiming, f32 badTiming)
    {
        GoodTiming = goodTiming;
        OkTiming = okTiming;
        BadTiming = badTiming;
    }

    void Header::ResetJudgeTiming(Difficulty difficulty)
    {
        switch (difficulty)
        {
        case Difficulty::Easy:
        case Difficulty::Normal:
            ResetJudgeTiming(41.70833588f, 108.4416656f, 125.125f);
            break;
        case Difficulty::Hard:
        case Difficulty::Oni:
        case Difficulty::Ura:
            ResetJudgeTiming(25.0250015f, 75.0750046f, 108.441666f);
            break;
        default:
            // Do nothing for other difficulties.
            break;
        }
    }

    void Header::ResetJudgeTiming(f32 goodTiming, f32 okTiming, f32 badTiming)
    {
        for (size_t i = 0; i < 36; ++i)
            JudgeTimings[i].ResetJudgeTiming(goodTiming, okTiming, badTiming);
    }

    // ============================================================================
    // FumenChart Implementation
    // ============================================================================

    Difficulty FumenChart::GetDifficulty() const
    {
        // Try to infer difficulty from clear HP values
        if (ChartHeader.ClearHP == ClearHP_Easy)
            return Difficulty::Easy;
        else if (ChartHeader.ClearHP == ClearHP_Normal_Hard)
        {
            // Cannot distinguish between Normal and Hard, default to Normal
            return Difficulty::Normal;
        }
        else if (ChartHeader.ClearHP == ClearHP_Oni)
            return Difficulty::Oni;

        // Default to Oni if cannot determine
        return Difficulty::Oni;
    }

    void FumenChart::AddMeasure(const Measure &measure)
    {
        Measures.push_back(measure);
    }

    void FumenChart::Clear()
    {
        std::memset(&ChartHeader, 0, sizeof(ChartHeader));
        Measures.clear();
    }

    // ============================================================================
    // FumenChartReader Implementation
    // ============================================================================

    template <typename T>
    void FumenChartReader::ReadData(const u8 *&data, const u8 *dataEnd, T &outValue, bool peek)
    {
        if (data + sizeof(T) > dataEnd)
        {
            ThrowError("Unexpected end of data while reading " + std::to_string(sizeof(T)) + " bytes", data);
        }

        std::memcpy(&outValue, data, sizeof(T));

        // #ifdef PEEPO_DEBUG
        //         std::cout << "ReadData: " << typeid(T).name() << " (" << sizeof(T) << " bytes) at offset 0x" << std::hex << (data - m_DataStart) << std::dec << std::endl;
        // #endif // PEEPO_DEBUG

        if (!peek)
            data += sizeof(T);
    }

    void FumenChartReader::ReadHeader(const u8 *&data, const u8 *dataEnd, Header &outHeader)
    {
        if (data + sizeof(Header) > dataEnd)
        {
            ThrowError("Not enough data to read header (expected " + std::to_string(sizeof(Header)) + " bytes)", data);
        }

        std::memcpy(&outHeader, data, sizeof(Header));
        data += sizeof(Header);
    }

    void FumenChartReader::ReadMeasureNotes(const u8 *&data, const u8 *dataEnd, std::vector<NoteData> &outNotes, f32 &scrollSpeed)
    {
        MeasureNotesData notesData;
        ReadData(data, dataEnd, notesData);

        scrollSpeed = notesData.ScrollSpeed;

        outNotes.clear();
        outNotes.reserve(notesData.NumberOfNotes);

        for (u16 i = 0; i < notesData.NumberOfNotes; ++i)
        {
            NoteData note;
            try
            {
                ReadData(data, dataEnd, note);

                if (note.isRendaNote())
                {
                    // Renda notes have 8 bytes of padding after them
                    u64 padding;
                    ReadData(data, dataEnd, padding);
                }
            }
            catch (const FumenParseException &e)
            {
                ThrowError("Failed to read note " + std::to_string(i + 1) + "/" + std::to_string(notesData.NumberOfNotes) + ": " + e.GetReason(), data);
            }

            outNotes.push_back(note);
        }
    }

    void FumenChartReader::ReadMeasure(const u8 *&data, const u8 *dataEnd, Measure &outMeasure)
    {
        // Read measure data
        try
        {
            ReadData(data, dataEnd, outMeasure.Data);
        }
        catch (const FumenParseException &e)
        {
            ThrowError("Failed to read measure " + std::to_string(m_CurrentMeasureIndex) + " data: " + e.GetReason(), data);
        }

        // #ifdef PEEPO_DEBUG
        //         if (outMeasure.Data.BPM <= 0.0f)
        //         {
        //             ThrowError("Invalid BPM value: " + std::to_string(outMeasure.Data.BPM), data);
        //         }
        // #endif // PEEPO_DEBUG

        // Read notes for all three branch paths
        try
        {
            ReadMeasureNotes(data, dataEnd, outMeasure.NormalNotes, outMeasure.NormalNotesScrollSpeed);
        }
        catch (const FumenParseException &e)
        {
            ThrowError("Failed to read measure " + std::to_string(m_CurrentMeasureIndex) + " normal notes: " + e.GetReason(), data);
        }

        try
        {
            ReadMeasureNotes(data, dataEnd, outMeasure.AdvancedNotes, outMeasure.AdvancedNotesScrollSpeed);
        }
        catch (const FumenParseException &e)
        {
            ThrowError("Failed to read measure " + std::to_string(m_CurrentMeasureIndex) + " advanced notes: " + e.GetReason(), data);
        }

        try
        {
            ReadMeasureNotes(data, dataEnd, outMeasure.MasterNotes, outMeasure.MasterNotesScrollSpeed);
        }
        catch (const FumenParseException &e)
        {
            ThrowError("Failed to read measure " + std::to_string(m_CurrentMeasureIndex) + " master notes: " + e.GetReason(), data);
        }
    }

    void FumenChartReader::ReadFromMemory(const u8 *data, size_t dataSize, FumenChart &outChart)
    {
        if (!data || dataSize == 0)
        {
            throw FumenParseException("Invalid input data: null pointer or zero size", 0);
        }

        m_DataStart = data;
        const u8 *dataEnd = data + dataSize;

        outChart.Clear();

        // Read header
        ReadHeader(data, dataEnd, outChart.ChartHeader);

        // Read measures
        u32 numMeasures = outChart.ChartHeader.NumberOfMeasures;
        outChart.Measures.reserve(numMeasures);

        for (u32 i = 0; i < numMeasures; ++i)
        {
            m_CurrentMeasureIndex = i;
            Measure measure;
            ReadMeasure(data, dataEnd, measure);
            outChart.AddMeasure(measure);
        }
    }

    void FumenChartReader::ReadFromFile(const std::string &filePath, FumenChart &outChart)
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + filePath);
        }

        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        auto buffer = new u8[fileSize];
        defer { delete[] buffer; };

        if (!file.read(reinterpret_cast<char *>(buffer), fileSize))
        {
            throw std::runtime_error("Failed to read file: " + filePath);
        }

        try
        {
            ReadFromMemory(buffer, fileSize, outChart);
        }
        catch (const FumenParseException &e)
        {
            throw FumenParseException("In file \"" + filePath + "\": " + e.GetReason(), e.GetOffset());
        }
    }

    // ============================================================================
    // FumenChartWriter Implementation
    // ============================================================================

    template <typename T>
    void FumenChartWriter::WriteData(std::vector<u8> &buffer, const T &value)
    {
        const u8 *dataPtr = reinterpret_cast<const u8 *>(&value);
        buffer.insert(buffer.end(), dataPtr, dataPtr + sizeof(T));
    }

    void FumenChartWriter::WriteHeader(std::vector<u8> &buffer, const Header &header)
    {
        const u8 *dataPtr = reinterpret_cast<const u8 *>(&header);
        buffer.insert(buffer.end(), dataPtr, dataPtr + sizeof(Header));
    }

    void FumenChartWriter::WriteMeasureNotes(std::vector<u8> &buffer, const std::vector<NoteData> &notes, f32 scrollSpeed)
    {
        MeasureNotesData notesData;
        notesData.NumberOfNotes = static_cast<u16>(notes.size());
        notesData._Padding1 = 0;
        notesData.ScrollSpeed = scrollSpeed;

        WriteData(buffer, notesData);

        for (const auto &note : notes)
        {
            WriteData(buffer, note);
            if (note.isRendaNote())
            {
                // Renda notes have 8 bytes of padding after them
                u64 padding = 0;
                WriteData(buffer, padding);
            }
        }
    }

    void FumenChartWriter::WriteMeasure(std::vector<u8> &buffer, const Measure &measure)
    {
        WriteData(buffer, measure.Data);

        // Write notes for all three branch paths
        // Use default scroll speed of 1.0 if not specified
        WriteMeasureNotes(buffer, measure.NormalNotes, 1.0f);
        WriteMeasureNotes(buffer, measure.AdvancedNotes, 1.0f);
        WriteMeasureNotes(buffer, measure.MasterNotes, 1.0f);
    }

    std::vector<u8> FumenChartWriter::WriteToMemory(const FumenChart &chart)
    {
        std::vector<u8> buffer;
        buffer.reserve(sizeof(Header) + chart.GetMeasureCount() * 1024); // Rough estimate

        WriteHeader(buffer, chart.ChartHeader);

        for (const auto &measure : chart.Measures)
        {
            WriteMeasure(buffer, measure);
        }

        return buffer;
    }

    void FumenChartWriter::WriteToFile(const std::string &filePath, const FumenChart &chart)
    {
        std::vector<u8> buffer = WriteToMemory(chart);

        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            throw FumenWriteException("Failed to open file for writing", filePath);
        }

        file.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
        if (!file.good())
        {
            throw FumenWriteException("Failed to write data to file", filePath);
        }
    }
}
