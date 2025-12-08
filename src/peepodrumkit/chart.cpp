#include "chart.h"
#include "core_build_info.h"
#include <algorithm>

namespace PeepoDrumKit
{
	void DebugCompareCharts(const ChartProject& chartA, const ChartProject& chartB, DebugCompareChartsOnMessageFunc onMessageFunc, void* userData)
	{
		auto logf = [onMessageFunc, userData](cstr fmt, ...)
		{
			char buffer[512];
			va_list args;
			va_start(args, fmt);
			onMessageFunc(std::string_view(buffer, vsnprintf(buffer, ArrayCount(buffer), fmt, args)), userData);
			va_end(args);
		};

		if (chartA.Courses.size() != chartB.Courses.size()) { logf("Course count mismatch (%zu != %zu)", chartA.Courses.size(), chartB.Courses.size()); return; }

		for (size_t i = 0; i < chartA.Courses.size(); i++)
		{
			const ChartCourse& courseA = *chartA.Courses[i];
			const ChartCourse& courseB = *chartB.Courses[i];

			for (GenericList list = {}; list < GenericList::Count; IncrementEnum(list))
			{
				const size_t countA = GetGenericListCount(courseA, list);
				const size_t countB = GetGenericListCount(courseB, list);
				if (countA != countB) { logf("%s count mismatch (%zu != %zu)", GenericListNames[EnumToIndex(list)], countA, countB); continue; }

				for (size_t itemIndex = 0; itemIndex < countA; itemIndex++)
				{
					for (GenericMember member = {}; member < GenericMember::Count; IncrementEnum(member))
					{
						GenericMemberUnion valueA {}, valueB {};
						const b8 hasValueA = TryGet(courseA, list, itemIndex, member, valueA);
						const b8 hasValueB = TryGet(courseB, list, itemIndex, member, valueB);
						assert(hasValueA == hasValueB);
						if (!hasValueA || member == GenericMember::B8_IsSelected)
							continue;

						static constexpr auto safeCStrAreSame = [](cstr a, cstr b) -> b8 { if ((a == nullptr) || (b == nullptr) && a != b) return false; return (strcmp(a, b) == 0); };
						cstr memberName = ""; b8 isSame = false;
						switch (member)
						{
						case GenericMember::B8_IsSelected: { memberName = "IsSelected"; isSame = (valueA.B8 == valueB.B8); } break;
						case GenericMember::B8_BarLineVisible: { memberName = "IsVisible"; isSame = (valueA.B8 == valueB.B8); } break;
						case GenericMember::I16_BalloonPopCount: { memberName = "BalloonPopCount"; isSame = (valueA.I16 == valueB.I16); } break;
						case GenericMember::F32_ScrollSpeed: { memberName = "ScrollSpeed"; isSame = ApproxmiatelySame(valueA.CPX, valueB.CPX); } break;
						case GenericMember::Beat_Start: { memberName = "BeatStart"; isSame = (valueA.Beat == valueB.Beat); } break;
						case GenericMember::Beat_Duration: { memberName = "BeatDuration"; isSame = (valueA.Beat == valueB.Beat); } break;
						case GenericMember::Time_Offset: { memberName = "TimeOffset"; isSame = ApproxmiatelySame(valueA.Time.Seconds, valueB.Time.Seconds); } break;
						case GenericMember::NoteType_V: { memberName = "NoteType"; isSame = (valueA.NoteType == valueB.NoteType); } break;
						case GenericMember::Tempo_V: { memberName = "Tempo"; isSame = ApproxmiatelySame(valueA.Tempo.BPM, valueB.Tempo.BPM); } break;
						case GenericMember::TimeSignature_V: { memberName = "TimeSignature"; isSame = (valueA.TimeSignature == valueB.TimeSignature); } break;
						case GenericMember::CStr_Lyric: { memberName = "Lyric"; isSame = safeCStrAreSame(valueA.CStr, valueB.CStr); } break;
						case GenericMember::I8_ScrollType: { memberName = "ScrollType"; isSame = (valueA.I16 == valueB.I16); } break;
						}

						if (!isSame)
							logf("%s[%zu].%s value mismatch", GenericListNames[EnumToIndex(list)], itemIndex, memberName);
					}
				}
			}
		}
	}

	struct TempTimedDelayCommand { Beat Beat; Time Delay; };

	template <>
	struct IsNonListChartEventTrait<TempTimedDelayCommand> : std::true_type { };

	template <GenericMember Member, typename TempTimedDelayCommandT, expect_type_t<TempTimedDelayCommandT, TempTimedDelayCommand> = true>
	constexpr decltype(auto) get(TempTimedDelayCommandT&& event)
	{
		if constexpr (Member == GenericMember::Beat_Start) return (std::forward<TempTimedDelayCommandT>(event).Beat);
	}
	
	using FumenNoteType = Fumen::FormatV2::NoteType;
	static constexpr NoteType ConvertFumenNoteType(FumenNoteType fumenNoteType)
	{
		switch (fumenNoteType)
		{
		case FumenNoteType::NoteType_None: return NoteType::Count;
		case FumenNoteType::NoteType_Don: return NoteType::Don;
		case FumenNoteType::NoteType_Do: return NoteType::Don;
		case FumenNoteType::NoteType_BigDon: return NoteType::DonBig;
		case FumenNoteType::NoteType_Ka: return NoteType::Ka;
		case FumenNoteType::NoteType_Ko: return NoteType::Ka;
		case FumenNoteType::NoteType_Katsu: return NoteType::Ka;
		case FumenNoteType::NoteType_BigKatsu: return NoteType::KaBig;
		case FumenNoteType::NoteType_Renda: return NoteType::Drumroll;
		case FumenNoteType::NoteType_BigRenda: return NoteType::DrumrollBig;
		case FumenNoteType::NoteType_Balloon: return NoteType::Balloon;
		case FumenNoteType::NoteType_Bell: return NoteType::BalloonSpecial;
		}
	}
	
	static constexpr FumenNoteType ConvertFumenNoteType(NoteType noteType)
	{
		switch (noteType)
		{
		case NoteType::Don: return FumenNoteType::NoteType_Don;
		case NoteType::DonBig: return FumenNoteType::NoteType_BigDon;
		case NoteType::Ka: return FumenNoteType::NoteType_Ka;
		case NoteType::KaBig: return FumenNoteType::NoteType_BigKatsu;
		case NoteType::Drumroll: return FumenNoteType::NoteType_Renda;
		case NoteType::DrumrollBig: return FumenNoteType::NoteType_BigRenda;
		case NoteType::Balloon: return FumenNoteType::NoteType_Balloon;
		case NoteType::BalloonSpecial: return FumenNoteType::NoteType_Bell;
		default: return FumenNoteType::NoteType_None;
		}
	}

	static constexpr NoteType ConvertTJANoteType(TJA::NoteType tjaNoteType)
	{
		switch (tjaNoteType)
		{
		case TJA::NoteType::None: return NoteType::Count;
		case TJA::NoteType::Don: return NoteType::Don;
		case TJA::NoteType::Ka: return NoteType::Ka;
		case TJA::NoteType::DonBig: return NoteType::DonBig;
		case TJA::NoteType::KaBig: return NoteType::KaBig;
		case TJA::NoteType::Start_Drumroll: return NoteType::Drumroll;
		case TJA::NoteType::Start_DrumrollBig: return NoteType::DrumrollBig;
		case TJA::NoteType::Start_Balloon: return NoteType::Balloon;
		case TJA::NoteType::End_BalloonOrDrumroll: return NoteType::Count;
		case TJA::NoteType::Start_BaloonSpecial: return NoteType::BalloonSpecial;
		case TJA::NoteType::DonBigBoth: return NoteType::DonBigHand;
		case TJA::NoteType::KaBigBoth: return NoteType::KaBigHand;
		case TJA::NoteType::Hidden: return NoteType::Adlib;
		case TJA::NoteType::Bomb: return NoteType::Bomb;
		case TJA::NoteType::KaDon: return NoteType::KaDon;
		case TJA::NoteType::Fuse: return NoteType::Fuse;
		default: return NoteType::Count;
		}
	}

	static constexpr TJA::NoteType ConvertTJANoteType(NoteType noteType)
	{
		switch (noteType)
		{
		case NoteType::Don: return TJA::NoteType::Don;
		case NoteType::DonBig: return TJA::NoteType::DonBig;
		case NoteType::Ka: return TJA::NoteType::Ka;
		case NoteType::KaBig: return TJA::NoteType::KaBig;
		case NoteType::Drumroll: return TJA::NoteType::Start_Drumroll;
		case NoteType::DrumrollBig: return TJA::NoteType::Start_DrumrollBig;
		case NoteType::Balloon: return TJA::NoteType::Start_Balloon;
		case NoteType::BalloonSpecial: return TJA::NoteType::Start_BaloonSpecial;
		case NoteType::DonBigHand: return TJA::NoteType::DonBigBoth;
		case NoteType::KaBigHand: return TJA::NoteType::KaBigBoth;
		case NoteType::KaDon: return TJA::NoteType::KaDon;
		case NoteType::Adlib: return TJA::NoteType::Hidden;
		case NoteType::Fuse: return TJA::NoteType::Fuse;
		case NoteType::Bomb: return TJA::NoteType::Bomb;
		default: return TJA::NoteType::None;
		}
	}

	static void AddBestSignatureDenominatorMatchesTime(ChartCourse& course, Time targetTime)
	{
		assert(!course.TempoMap.Signature.Sorted.empty());
		assert(!course.TempoMap.Tempo.Sorted.empty());

		auto &lastBPMChange = course.TempoMap.Tempo.Sorted.back();
		auto &lastSignatureChange = course.TempoMap.Signature.Sorted.back();
		auto timeDuration = (course.TempoMap.BeatToTime(lastSignatureChange.Beat) - targetTime).ToMS_F32();
		struct DenominatorMatch {
			int Denominator;
			f32 TimeDelta;
		};
		std::optional<DenominatorMatch> bestDenominator;

		for (int i = 1; i < 1024; i++)
		{
			auto nextBarlineDuration = (60000.0f / lastBPMChange.Tempo.BPM) * (4.0f / static_cast<f32>(i) * 4.0f);
			auto nextBarlineToTimeCount = Round(timeDuration / nextBarlineDuration);
			auto matchBarlineDelta = Absolute(timeDuration - (nextBarlineDuration * nextBarlineToTimeCount));
			if (ApproxmiatelySame(matchBarlineDelta, 0.0f))
			{
				bestDenominator = { i, matchBarlineDelta };
				break;
			}
			else if (!bestDenominator.has_value() || matchBarlineDelta < bestDenominator->TimeDelta)
			{
				bestDenominator = { i, matchBarlineDelta };
			}
		}

		if (bestDenominator.has_value())
		{
			lastSignatureChange.Signature.Denominator = bestDenominator->Denominator;
			lastSignatureChange.Signature.Simplify();
			auto hideBarBeat = lastSignatureChange.Beat + lastSignatureChange.Signature.GetDurationPerBeat();
			course.BarLineChanges.Sorted.emplace_back(BarLineChange { hideBarBeat, false });
		}
	}

	Beat FindCourseMaxUsedBeat(const ChartCourse& course)
	{
		// NOTE: Technically only need to look at the last item of each sorted list **but just to be sure**, in case there is something wonky going on with out-of-order durations or something
		Beat maxBeat = Beat::Zero();
		for (const auto& v : course.TempoMap.Tempo) maxBeat = Max(maxBeat, v.Beat);
		for (const auto& v : course.TempoMap.Signature) maxBeat = Max(maxBeat, v.Beat);
		for (size_t i = 0; i < EnumCount<BranchType>; i++)
			for (const auto& v : course.GetNotes(static_cast<BranchType>(i))) maxBeat = Max(maxBeat, v.BeatTime + Max(Beat::Zero(), v.BeatDuration));
		for (const auto& v : course.GoGoRanges) maxBeat = Max(maxBeat, v.BeatTime + Max(Beat::Zero(), v.BeatDuration));
		for (const auto& v : course.ScrollChanges) maxBeat = Max(maxBeat, v.BeatTime);
		for (const auto& v : course.BarLineChanges) maxBeat = Max(maxBeat, v.BeatTime);
		for (const auto& v : course.Lyrics) maxBeat = Max(maxBeat, v.BeatTime);
		return maxBeat;
	}
	
	b8 CreateChartProjectFromFumen(const Fumen::FormatV2::FumenChart& inFumen, ChartProject& out)
	{
		out.ChartDuration = Time::Zero();
		
		b8 isBarlineVisible = inFumen.Measures.empty() ? true : static_cast<b8>(inFumen.Measures[0].Data.IsBarLineVisible);
		Tempo currentBpm = inFumen.Measures.empty() ? FallbackTempo : Tempo(inFumen.Measures[0].Data.BPM);
		f32 currentScrollSpeed = inFumen.Measures.empty() ? 1.0f : inFumen.Measures[0].NormalNotesScrollSpeed;
		f32 firstMeasureOffset = inFumen.Measures.empty() ? 0.0f : inFumen.Measures[0].Data.MeasureOffset;
		f32 bpmChangeOffsetAccum = 0.0f;
		auto gogoTimeStartMeasure = std::optional<Time>();

		out.Courses.clear();
		ChartCourse& course = *out.Courses.emplace_back(std::make_unique<ChartCourse>());

		course.TempoMap.Tempo.Sorted.emplace_back(TempoChange { Beat::Zero(), currentBpm });
		course.TempoMap.Signature.Sorted.emplace_back(TimeSignatureChange { Beat::Zero(), TimeSignature(4, 4) });
		course.TempoMap.RebuildAccelerationStructure();
		course.BarLineChanges.Sorted.emplace_back(BarLineChange { Beat::Zero(), isBarlineVisible });

		for (auto it = inFumen.Measures.begin(); it != inFumen.Measures.end(); ++it)
		{
			auto& measure = *it;
			auto pos = Time::FromMS(measure.Data.MeasureOffset - firstMeasureOffset + bpmChangeOffsetAccum);

			if (!ApproxmiatelySame(measure.Data.BPM, currentBpm.BPM))
			{
				f32 bpmChangeOffset = (4.0f * 60000.0f / currentBpm.BPM) - (4.0f * 60000.0f / measure.Data.BPM);
				bpmChangeOffsetAccum -= bpmChangeOffset;
				currentBpm = Tempo(measure.Data.BPM);
				pos = Time::FromMS(measure.Data.MeasureOffset - firstMeasureOffset + bpmChangeOffsetAccum);

				auto &lastSignature = course.TempoMap.Signature.Sorted.back();
				auto &lastTempo = course.TempoMap.Tempo.Sorted.back();
				auto measureDistance = pos - course.TempoMap.BeatToTime(lastSignature.Beat);

				AddBestSignatureDenominatorMatchesTime(course, pos);

				auto posBeat = course.TempoMap.TimeToBeat(pos);
				course.TempoMap.Tempo.Sorted.emplace_back(TempoChange { posBeat, currentBpm });
				course.TempoMap.Signature.Sorted.emplace_back(TimeSignatureChange { posBeat, TimeSignature(4, 4), });
				course.TempoMap.RebuildAccelerationStructure();
			} else if (std::distance(inFumen.Measures.begin(), it) > 0)
			{
				auto &lastSignature = course.TempoMap.Signature.Sorted.back();
				auto &lastTempo = course.TempoMap.Tempo.Sorted.back();
				auto measureDistance = pos - course.TempoMap.BeatToTime(lastSignature.Beat);

				AddBestSignatureDenominatorMatchesTime(course, pos);

				course.TempoMap.Signature.Sorted.emplace_back(TimeSignatureChange {
					course.TempoMap.TimeToBeat(pos),
					TimeSignature(4, 4),
				});
				course.TempoMap.RebuildAccelerationStructure();
			}

			if (!ApproxmiatelySame(measure.NormalNotesScrollSpeed, currentScrollSpeed))
			{
				currentScrollSpeed = measure.NormalNotesScrollSpeed;
				course.ScrollChanges.Sorted.emplace_back(ScrollChange { course.TempoMap.TimeToBeat(pos), Complex(currentScrollSpeed, 0) });
			}

			course.BarLineChanges.Sorted.emplace_back(BarLineChange { course.TempoMap.TimeToBeat(pos), static_cast<b8>(measure.Data.IsBarLineVisible) });

			if (gogoTimeStartMeasure.has_value())
			{
				if (measure.Data.IsGogoTime == 0)
				{
					auto startPos = *gogoTimeStartMeasure;
					course.GoGoRanges.Sorted.emplace_back(GoGoRange {
						.BeatTime = course.TempoMap.TimeToBeat(startPos),
						.BeatDuration = course.TempoMap.TimeToBeat(pos - startPos),
					});
					gogoTimeStartMeasure.reset();
				}
			}
			else if (measure.Data.IsGogoTime != 0)
			{
				gogoTimeStartMeasure = pos;
			}
			// TODO: Measure Division Data and correction of bar line position
			for (auto nit = measure.NormalNotes.begin(); nit != measure.NormalNotes.end(); ++nit)
			{
				auto& note = *nit;
				NoteType noteType = ConvertFumenNoteType(note.Type);
				if (noteType == NoteType::Count)
					continue;

				Note& outNote = course.Notes_Normal.Sorted.emplace_back();
				auto beginTime = pos + Time::FromMS(note.NoteOffset);
				outNote.BeatTime = course.TempoMap.TimeToBeat(beginTime);
				outNote.Type = noteType;
				outNote.TimeOffset = Time::Zero();

				if (note.Type == Fumen::FormatV2::NoteType::NoteType_Balloon || note.Type == Fumen::FormatV2::NoteType::NoteType_Bell)
				{
					if (note.InitialScoreValue == 0)
						outNote.BalloonPopCount = static_cast<i16>(note.BalloonHitCount_Old);
					else
						outNote.BalloonPopCount = static_cast<i16>(note.InitialScoreValue);
					outNote.BeatDuration = course.TempoMap.TimeToBeat(beginTime + Time::FromMS(note.Length)) - outNote.GetStart();
				}
				else if (note.isRendaNote())
				{
					outNote.BeatDuration = course.TempoMap.TimeToBeat(beginTime + Time::FromMS(note.Length)) - outNote.GetStart();
				}
			}
		}

		if (gogoTimeStartMeasure.has_value())
		{
			auto startPos = course.TempoMap.TimeToBeat(*gogoTimeStartMeasure);
			auto endPos = course.TempoMap.TimeToBeat(out.GetDurationOrDefault());
			course.GoGoRanges.Sorted.emplace_back(GoGoRange {
				.BeatTime = startPos,
				.BeatDuration = endPos - startPos,
			});
		}

		// Simplify barline visible commands
		std::vector<BarLineChange> simplifiedBarlineChangeEvents;
		b8 currentlyVisible = course.BarLineChanges.Sorted.empty() ? true : course.BarLineChanges.Sorted.front().IsVisible;
		simplifiedBarlineChangeEvents.emplace_back(BarLineChange { Beat::Zero(), currentlyVisible });
		for (auto it = course.BarLineChanges.Sorted.begin(); it != course.BarLineChanges.Sorted.end(); ++it)
		{
			if (currentlyVisible != it->IsVisible)
			{
				currentlyVisible = it->IsVisible;
				simplifiedBarlineChangeEvents.emplace_back(*it);
			}
		}
		course.BarLineChanges.Sorted = simplifiedBarlineChangeEvents;

		// Simplify time signature change events
		std::vector<TimeSignatureChange> simplifiedSignatureChangeEvents;
		TimeSignature currentSignature = course.TempoMap.Signature.Sorted.empty() ? TimeSignature(4, 4) : course.TempoMap.Signature.Sorted.front().Signature;
		simplifiedSignatureChangeEvents.emplace_back(TimeSignatureChange { Beat::Zero(), currentSignature });
		for (auto it = course.TempoMap.Signature.Sorted.begin(); it != course.TempoMap.Signature.Sorted.end(); ++it)
		{
			if (currentSignature != it->Signature)
			{
				currentSignature = it->Signature;
				simplifiedSignatureChangeEvents.emplace_back(*it);
			}
		}
		course.TempoMap.Signature.Sorted = simplifiedSignatureChangeEvents;

		// Simplify tempo change events
		std::vector<TempoChange> simplifiedTempoChangeEvents;
		Tempo currentTempo = course.TempoMap.Tempo.Sorted.empty() ? FallbackTempo : course.TempoMap.Tempo.Sorted.front().Tempo;
		simplifiedTempoChangeEvents.emplace_back(TempoChange { Beat::Zero(), currentTempo });
		for (auto it = course.TempoMap.Tempo.Sorted.begin(); it != course.TempoMap.Tempo.Sorted.end(); ++it)
		{
			if (!ApproxmiatelySame(currentTempo.BPM, it->Tempo.BPM))
			{
				currentTempo = it->Tempo;
				simplifiedTempoChangeEvents.emplace_back(*it);
			}
		}

		if (!inFumen.Measures.empty()) {
			auto & lastMeasure = inFumen.Measures.back();
			out.ChartDuration = Time::FromMS(lastMeasure.Data.MeasureOffset - firstMeasureOffset + bpmChangeOffsetAccum + 4.0f * 60000.0f / lastMeasure.Data.BPM);
		}

		return true;
	}

	b8 CreateChartProjectFromTJA(const TJA::ParsedTJA& inTJA, ChartProject& out)
	{
		out.ChartDuration = Time::Zero();
		out.ChartTitle = inTJA.Metadata.TITLE;
		out.ChartTitleLocalized = inTJA.Metadata.TITLE_localized;
		out.ChartSubtitle = inTJA.Metadata.SUBTITLE;
		out.ChartSubtitleLocalized = inTJA.Metadata.SUBTITLE_localized;
		out.ChartCreator = inTJA.Metadata.MAKER;
		out.ChartGenre = inTJA.Metadata.GENRE;
		out.ChartLyricsFileName = inTJA.Metadata.LYRICS;
		out.SongOffset = inTJA.Metadata.OFFSET;
		out.SongDemoStartTime = inTJA.Metadata.DEMOSTART;
		out.SongFileName = inTJA.Metadata.WAVE;
		out.SongJacket = inTJA.Metadata.PREIMAGE;
		out.SongVolume = inTJA.Metadata.SONGVOL;
		out.SoundEffectVolume = inTJA.Metadata.SEVOL;
		out.BackgroundImageFileName = inTJA.Metadata.BGIMAGE;
		out.BackgroundMovieFileName = inTJA.Metadata.BGMOVIE;
		out.MovieOffset = inTJA.Metadata.MOVIEOFFSET;
		out.OtherMetadata = inTJA.Metadata.Others;
		for (size_t i = 0; i < inTJA.Courses.size(); i++)
		{
			if (!inTJA.Courses[i].HasChart) // metadata-only TJA section
				continue;

			const TJA::ConvertedCourse& inCourse = TJA::ConvertParsedToConvertedCourse(inTJA, inTJA.Courses[i]);
			ChartCourse& outCourse = *out.Courses.emplace_back(std::make_unique<ChartCourse>());

			// HACK: Write proper enum conversion functions
			outCourse.Type = Clamp(static_cast<DifficultyType>(inCourse.CourseMetadata.COURSE), DifficultyType {}, DifficultyType::Count);
			outCourse.Level = Clamp(static_cast<DifficultyLevel>(inCourse.CourseMetadata.LEVEL), DifficultyLevel::Min, DifficultyLevel::Max);
			outCourse.Decimal = Clamp(static_cast<DifficultyLevelDecimal>(inCourse.CourseMetadata.LEVEL_DECIMALTAG), DifficultyLevelDecimal::None, DifficultyLevelDecimal::Max);
			outCourse.Style = std::max(inCourse.CourseMetadata.STYLE, 1);
			outCourse.PlayerSide = std::clamp(inCourse.CourseMetadata.START_PLAYERSIDE, 1, outCourse.Style);

			outCourse.CourseCreator = inCourse.CourseMetadata.NOTESDESIGNER;

			outCourse.Life = Clamp(static_cast<TowerLives>(inCourse.CourseMetadata.LIFE), TowerLives::Min, TowerLives::Max);
			outCourse.Side = Clamp(static_cast<Side>(inCourse.CourseMetadata.SIDE), Side{}, Side::Count);

			outCourse.TempoMap.Tempo.Sorted = { TempoChange(Beat::Zero(), inTJA.Metadata.BPM) };
			outCourse.TempoMap.Signature.Sorted = { TimeSignatureChange(Beat::Zero(), TimeSignature(4, 4)) };
			TimeSignature lastSignature = TimeSignature(4, 4);

			i32 currentBalloonIndex = 0;

			BeatSortedList<TempTimedDelayCommand> tempSortedDelayCommands;
			BeatSortedForwardIterator<TempTimedDelayCommand> tempDelayCommandsIt;
			for (const TJA::ConvertedMeasure& inMeasure : inCourse.Measures)
			{
				for (const TJA::ConvertedDelayChange& inDelayChange : inMeasure.DelayChanges)
					tempSortedDelayCommands.InsertOrUpdate(TempTimedDelayCommand { inMeasure.StartTime + inDelayChange.TimeWithinMeasure, inDelayChange.Delay });
			}

			for (const TJA::ConvertedMeasure& inMeasure : inCourse.Measures)
			{
				for (const TJA::ConvertedNote& inNote : inMeasure.Notes)
				{
					if (inNote.Type == TJA::NoteType::End_BalloonOrDrumroll)
					{
						// TODO: Proper handling
						if (!outCourse.Notes_Normal.Sorted.empty())
							outCourse.Notes_Normal.Sorted.back().BeatDuration = (inMeasure.StartTime + inNote.TimeWithinMeasure) - outCourse.Notes_Normal.Sorted.back().BeatTime;
						continue;
					}

					const NoteType outNoteType = ConvertTJANoteType(inNote.Type);
					if (outNoteType == NoteType::Count)
						continue;

					Note& outNote = outCourse.Notes_Normal.Sorted.emplace_back();
					outNote.BeatTime = (inMeasure.StartTime + inNote.TimeWithinMeasure);
					outNote.Type = outNoteType;

					const TempTimedDelayCommand* delayCommandForThisNote = tempDelayCommandsIt.Next(tempSortedDelayCommands.Sorted, outNote.BeatTime);
					outNote.TimeOffset = (delayCommandForThisNote != nullptr) ? delayCommandForThisNote->Delay : Time::Zero();

					if (inNote.Type == TJA::NoteType::Start_Balloon || inNote.Type == TJA::NoteType::Start_BaloonSpecial || inNote.Type == TJA::NoteType::Fuse)
					{
						// TODO: Implement properly with correct branch handling
						if (InBounds(currentBalloonIndex, inCourse.CourseMetadata.BALLOON))
							outNote.BalloonPopCount = inCourse.CourseMetadata.BALLOON[currentBalloonIndex];
						currentBalloonIndex++;
					}
				}

				if (inMeasure.TimeSignature != lastSignature)
				{
					outCourse.TempoMap.Signature.InsertOrUpdate(TimeSignatureChange(inMeasure.StartTime, inMeasure.TimeSignature));
					lastSignature = inMeasure.TimeSignature;
				}

				for (const TJA::ConvertedTempoChange& inTempoChange : inMeasure.TempoChanges)
					outCourse.TempoMap.Tempo.InsertOrUpdate(TempoChange(inMeasure.StartTime + inTempoChange.TimeWithinMeasure, inTempoChange.Tempo));

				for (const TJA::ConvertedScrollChange& inScrollChange : inMeasure.ScrollChanges)
					outCourse.ScrollChanges.Sorted.push_back(ScrollChange { (inMeasure.StartTime + inScrollChange.TimeWithinMeasure), inScrollChange.ScrollSpeed });

				for (const TJA::ConvertedScrollType& inScrollType : inMeasure.ScrollTypes)
					outCourse.ScrollTypes.Sorted.push_back(ScrollType{ (inMeasure.StartTime + inScrollType.TimeWithinMeasure),  static_cast<ScrollMethod>(inScrollType.Method) });

				for (const TJA::ConvertedJPOSScroll& inJPOSScrollChange : inMeasure.JPOSScrollChanges)
					outCourse.JPOSScrollChanges.Sorted.push_back(JPOSScrollChange{ (inMeasure.StartTime + inJPOSScrollChange.TimeWithinMeasure), inJPOSScrollChange.Move, inJPOSScrollChange.Duration });


				for (const TJA::ConvertedBarLineChange& barLineChange : inMeasure.BarLineChanges)
					outCourse.BarLineChanges.Sorted.push_back(BarLineChange { (inMeasure.StartTime + barLineChange.TimeWithinMeasure), barLineChange.Visibile });

				for (const TJA::ConvertedLyricChange& lyricChange : inMeasure.LyricChanges)
					outCourse.Lyrics.Sorted.push_back(LyricChange { (inMeasure.StartTime + lyricChange.TimeWithinMeasure), lyricChange.Lyric });
			}

			for (const TJA::ConvertedGoGoRange& inGoGoRange : inCourse.GoGoRanges)
				outCourse.GoGoRanges.Sorted.push_back(GoGoRange { inGoGoRange.StartTime, (inGoGoRange.EndTime - inGoGoRange.StartTime) });

			//outCourse.TempoMap.SetTempoChange(TempoChange());
			//outCourse.TempoMap = inCourse.GoGoRanges;

			outCourse.ScoreInit = inCourse.CourseMetadata.SCOREINIT;
			outCourse.ScoreDiff = inCourse.CourseMetadata.SCOREDIFF;

			outCourse.OtherMetadata = inCourse.CourseMetadata.Others;

			outCourse.TempoMap.RebuildAccelerationStructure();
			outCourse.RecalculateSENotes();

			if (!inCourse.Measures.empty())
				out.ChartDuration = Max(out.ChartDuration, outCourse.TempoMap.BeatToTime(inCourse.Measures.back().StartTime /*+ inCourse.Measures.back().TimeSignature.GetDurationPerBar()*/));
		}

		return true;
	}

	b8 ConvertChartProjectToTJA(const ChartProject& in, TJA::ParsedTJA& out, b8 includePeepoDrumKitComment)
	{
		static constexpr cstr FallbackTJAChartTitle = "Untitled Chart";
		out.Metadata.TITLE = !in.ChartTitle.empty() ? in.ChartTitle : FallbackTJAChartTitle;
		out.Metadata.TITLE_localized = in.ChartTitleLocalized;
		out.Metadata.SUBTITLE = in.ChartSubtitle;
		out.Metadata.SUBTITLE_localized = in.ChartSubtitleLocalized;
		out.Metadata.MAKER = in.ChartCreator;
		out.Metadata.GENRE = in.ChartGenre;
		out.Metadata.LYRICS = in.ChartLyricsFileName;
		out.Metadata.OFFSET = in.SongOffset;
		out.Metadata.DEMOSTART = in.SongDemoStartTime;
		out.Metadata.WAVE = in.SongFileName;
		out.Metadata.PREIMAGE = in.SongJacket;
		out.Metadata.SONGVOL = in.SongVolume;
		out.Metadata.SEVOL = in.SoundEffectVolume;
		out.Metadata.BGIMAGE = in.BackgroundImageFileName;
		out.Metadata.BGMOVIE = in.BackgroundMovieFileName;
		out.Metadata.MOVIEOFFSET = in.MovieOffset;
		out.Metadata.Others = in.OtherMetadata;

		if (includePeepoDrumKitComment)
		{
			out.HasPeepoDrumKitComment = true;
			out.PeepoDrumKitCommentDate = BuildInfo::CompilationDateParsed;
		}

		if (!in.Courses.empty())
		{
			if (!in.Courses[0]->TempoMap.Tempo.empty())
			{
				const TempoChange* initialTempo = in.Courses[0]->TempoMap.Tempo.TryFindLastAtBeat(Beat::Zero());
				out.Metadata.BPM = (initialTempo != nullptr) ? initialTempo->Tempo : FallbackTempo;
			}
		}

		out.Courses.reserve(in.Courses.size());
		for (const std::unique_ptr<ChartCourse>& inCourseIt : in.Courses)
		{
			const ChartCourse& inCourse = *inCourseIt;
			TJA::ParsedCourse& outCourse = out.Courses.emplace_back();

			// HACK: Write proper enum conversion functions
			outCourse.Metadata.COURSE = static_cast<TJA::DifficultyType>(inCourse.Type);
			outCourse.Metadata.LEVEL = static_cast<i32>(inCourse.Level);
			outCourse.Metadata.LEVEL_DECIMALTAG = static_cast<i32>(inCourse.Decimal);
			outCourse.Metadata.STYLE = inCourse.Style;
			outCourse.Metadata.START_PLAYERSIDE = inCourse.PlayerSide;
			outCourse.Metadata.NOTESDESIGNER = inCourse.CourseCreator;
			for (const Note& inNote : inCourse.Notes_Normal) if (IsBalloonNote(inNote.Type)) { outCourse.Metadata.BALLOON.push_back(inNote.BalloonPopCount); }
			outCourse.Metadata.SCOREINIT = inCourse.ScoreInit;
			outCourse.Metadata.SCOREDIFF = inCourse.ScoreDiff;

			outCourse.Metadata.LIFE = static_cast<i32>(inCourse.Life);
			outCourse.Metadata.SIDE = static_cast<TJA::SongSelectSide>(inCourse.Side);

			outCourse.Metadata.Others = inCourse.OtherMetadata;

			// TODO: Is this implemented correctly..? Need to have enough measures to cover every note/command and pad with empty measures up to the chart duration
			// BUG: NOPE! "07 �Q�[���~���[�W�b�N/003D. MagiCatz/MagiCatz.tja" for example still gets rounded up and then increased by a measure each time it gets saved
			// ... and even so does "Heat Haze Shadow 2.tja" without any weird time signatures..??
			const Beat inChartMaxUsedBeat = FindCourseMaxUsedBeat(inCourse);
			const Beat inChartBeatDuration = inCourse.TempoMap.TimeToBeat(in.GetDurationOrDefault());
			std::vector<TJA::ConvertedMeasure> outConvertedMeasures;

			inCourse.TempoMap.ForEachBeatBar([&](const SortedTempoMap::ForEachBeatBarData& it)
			{
				if (inChartBeatDuration > inChartMaxUsedBeat && (it.Beat >= inChartBeatDuration))
					return ControlFlow::Break;
				if (it.IsBar)
				{
					TJA::ConvertedMeasure& outConvertedMeasure = outConvertedMeasures.emplace_back();
					outConvertedMeasure.StartTime = it.Beat;
					outConvertedMeasure.TimeSignature = it.Signature;
				}
				return (it.Beat >= Max(inChartBeatDuration, inChartMaxUsedBeat)) ? ControlFlow::Break : ControlFlow::Fallthrough;
			});

			if (outConvertedMeasures.empty())
				outConvertedMeasures.push_back(TJA::ConvertedMeasure { Beat::Zero(), TimeSignature(4, 4) });

			static constexpr auto tryFindMeasureForBeat = [](std::vector<TJA::ConvertedMeasure>& measures, Beat beatToFind) -> TJA::ConvertedMeasure*
			{
				static constexpr auto isMoreBeat = [](const TJA::ConvertedMeasure& lhs, const TJA::ConvertedMeasure& rhs)
				{
					return lhs.StartTime > rhs.StartTime;
				};
				// Binary search in descending (ascending but reversed) list
				// if found: `it` is the last element such that `beatToFind >= it->StartTime`
				auto it = std::lower_bound(measures.rbegin(), measures.rend(), TJA::ConvertedMeasure { beatToFind }, isMoreBeat);
				return (it == measures.rend()) ? nullptr : &*it;
			};

			for (const TempoChange& inTempoChange : inCourse.TempoMap.Tempo)
			{
				if (!(&inTempoChange == &inCourse.TempoMap.Tempo[0] && inTempoChange.Tempo.BPM == out.Metadata.BPM.BPM))
				{
					TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inTempoChange.Beat);
					if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
						outConvertedMeasure->TempoChanges.push_back(TJA::ConvertedTempoChange { (inTempoChange.Beat - outConvertedMeasure->StartTime), inTempoChange.Tempo });
				}
			}

			Time lastNoteTimeOffset = Time::Zero();
			for (const Note& inNote : inCourse.Notes_Normal)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inNote.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->Notes.push_back(TJA::ConvertedNote { (inNote.BeatTime - outConvertedMeasure->StartTime), ConvertTJANoteType(inNote.Type) });

				if (inNote.BeatDuration > Beat::Zero())
				{
					TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inNote.BeatTime + inNote.BeatDuration);
					if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
						outConvertedMeasure->Notes.push_back(TJA::ConvertedNote { ((inNote.BeatTime + inNote.BeatDuration) - outConvertedMeasure->StartTime), TJA::NoteType::End_BalloonOrDrumroll });
				}

				const Time thisNoteTimeOffset = ApproxmiatelySame(inNote.TimeOffset.Seconds, 0.0) ? Time::Zero() : inNote.TimeOffset;
				if (thisNoteTimeOffset != lastNoteTimeOffset)
				{
					outConvertedMeasure->DelayChanges.push_back(TJA::ConvertedDelayChange { (inNote.BeatTime - outConvertedMeasure->StartTime), thisNoteTimeOffset });
					lastNoteTimeOffset = thisNoteTimeOffset;
				}
			}

			for (const ScrollChange& inScroll : inCourse.ScrollChanges)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inScroll.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->ScrollChanges.push_back(TJA::ConvertedScrollChange { (inScroll.BeatTime - outConvertedMeasure->StartTime), inScroll.ScrollSpeed });
			}

			for (const ScrollType& inScrollType : inCourse.ScrollTypes)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inScrollType.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->ScrollTypes.push_back(TJA::ConvertedScrollType { (inScrollType.BeatTime - outConvertedMeasure->StartTime), static_cast<i8>(inScrollType.Method) });
			}

			for (const JPOSScrollChange& JPOSScroll : inCourse.JPOSScrollChanges)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, JPOSScroll.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->JPOSScrollChanges.push_back(TJA::ConvertedJPOSScroll { (JPOSScroll.BeatTime - outConvertedMeasure->StartTime), JPOSScroll.Move, JPOSScroll.Duration });
			}

			for (const BarLineChange& barLineChange : inCourse.BarLineChanges)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, barLineChange.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->BarLineChanges.push_back(TJA::ConvertedBarLineChange { (barLineChange.BeatTime - outConvertedMeasure->StartTime), barLineChange.IsVisible });
			}

			for (const LyricChange& inLyric : inCourse.Lyrics)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inLyric.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->LyricChanges.push_back(TJA::ConvertedLyricChange { (inLyric.BeatTime - outConvertedMeasure->StartTime), inLyric.Lyric });
			}

			// For go-go time events, convert each range to a pair of start & end changes
			for (const GoGoRange& gogo : inCourse.GoGoRanges)
			{
				// start
				TJA::ConvertedMeasure* outConvertedMeasureStart = tryFindMeasureForBeat(outConvertedMeasures, gogo.BeatTime);
				if (assert(outConvertedMeasureStart != nullptr); outConvertedMeasureStart != nullptr)
					outConvertedMeasureStart->GoGoChanges.push_back(TJA::ConvertedGoGoChange{ (gogo.BeatTime - outConvertedMeasureStart->StartTime), true });
				// end
				const Beat endTime = gogo.BeatTime + Max(Beat::Zero(), gogo.BeatDuration);
				TJA::ConvertedMeasure* outConvertedMeasureEnd = tryFindMeasureForBeat(outConvertedMeasures, endTime);
				if (assert(outConvertedMeasureEnd != nullptr); outConvertedMeasureEnd != nullptr)
					outConvertedMeasureEnd->GoGoChanges.push_back(TJA::ConvertedGoGoChange{ (endTime - outConvertedMeasureEnd->StartTime), false });
			}

			TJA::ConvertConvertedMeasuresToParsedCommands(outConvertedMeasures, outCourse.ChartCommands);
		}

		return true;
	}
	
	static Fumen::FormatV2::Difficulty ConvertDifficultyTypeToFumenDifficulty(DifficultyType in)
	{
		using namespace Fumen::FormatV2;
		switch (in)
		{
		case DifficultyType::Easy:
			return Difficulty::Easy;
		case DifficultyType::Normal:
			return Difficulty::Normal;
		case DifficultyType::Hard:
			return Difficulty::Hard;
		case DifficultyType::Oni:
			return Difficulty::Oni;
		case DifficultyType::OniUra:
			return Difficulty::Ura;
		default:
			return Difficulty::Oni;
		}
	}

	b8 ConvertChartProjectToFumen(const ChartProject& in, Fumen::FormatV2::FumenChart& out, size_t targetCourseIndex)
	{
		using namespace Fumen::FormatV2;

		out.Clear();

		// Fumen 格式只支持单个难度,所以只取第一个 Course
		if (in.Courses.empty())
			return false;

		const ChartCourse& inCourse = *in.Courses[targetCourseIndex];

		// 初始化 Header
		out.ChartHeader.ResetJudgeTiming(ConvertDifficultyTypeToFumenDifficulty(inCourse.Type));
		out.ChartHeader.HasDivergentPaths = 0; // 简化实现,不支持分歧
		out.ChartHeader.MaxHP = 10000;
		
		// 根据难度设置 ClearHP
		switch (inCourse.Type)
		{
		case DifficultyType::Easy:
			out.ChartHeader.ClearHP = Fumen::ClearHP_Easy;
			break;
		case DifficultyType::Normal:
		case DifficultyType::Hard:
			out.ChartHeader.ClearHP = Fumen::ClearHP_Normal_Hard;
			break;
		default:
			out.ChartHeader.ClearHP = Fumen::ClearHP_Oni;
			break;
		}

		// 设置 HP 相关参数
		out.ChartHeader.HPPerGood = 10;
		out.ChartHeader.HPPerOk = 5;
		out.ChartHeader.HPPerBad = -20;
		out.ChartHeader.MaxCombo = 65536;
		out.ChartHeader.HPIncreaseRatio = 65536;
		out.ChartHeader.HPIncreaseRatioMaster = 65536;

		// 设置分歧点数 (虽然不使用分歧,但需要设置默认值)
		out.ChartHeader.GoodDivergePoints = 20;
		out.ChartHeader.OkDivergePoints = 10;
		out.ChartHeader.BadDivergePoints = 0;
		out.ChartHeader.DrumrollDivergePoints = 1;
		out.ChartHeader.GoodDivergePointsBig = 20;
		out.ChartHeader.OkDivergePointsBig = 10;
		out.ChartHeader.DrumrollDivergePointsBig = 1;
		out.ChartHeader.BalloonDivergePoints = 30;
		out.ChartHeader.BellYamDivergePoints = 30;
		out.ChartHeader.NumberOfDivergePoints = 20;

		// 设置判定时间
		out.ChartHeader.ResetJudgeTiming(static_cast<Difficulty>(inCourse.Type));

		// 遍历小节
		const Beat maxBeat = Max(FindCourseMaxUsedBeat(inCourse), inCourse.TempoMap.TimeToBeat(in.GetDurationOrDefault()));
		std::vector<Measure> measures;

		inCourse.TempoMap.ForEachBeatBar([&](const SortedTempoMap::ForEachBeatBarData& it)
		{
			if (it.Beat >= maxBeat)
				return ControlFlow::Break;

			if (it.IsBar)
			{
				Measure measure;
				const Time measureTime = inCourse.TempoMap.BeatToTime(it.Beat);
				
				// 设置 BPM
				const TempoChange* tempoChange = inCourse.TempoMap.Tempo.TryFindLastAtBeat(it.Beat);
				measure.Data.BPM = (tempoChange != nullptr) ? tempoChange->Tempo.BPM : FallbackTempo.BPM;

				// 设置 MeasureOffset (以毫秒为单位)
				measure.Data.MeasureOffset = measureTime.ToMS_F32();

				// 检查 GoGo Time
				measure.Data.IsGogoTime = 0;
				for (const GoGoRange& gogo : inCourse.GoGoRanges)
				{
					const Beat gogoEnd = gogo.BeatTime + gogo.BeatDuration;
					if (it.Beat >= gogo.BeatTime && it.Beat < gogoEnd)
					{
						measure.Data.IsGogoTime = 1;
						break;
					}
				}

				// 检查小节线可见性
				const BarLineChange* barLineChange = inCourse.BarLineChanges.TryFindLastAtBeat(it.Beat);
				measure.Data.IsBarLineVisible = (barLineChange != nullptr) ? barLineChange->IsVisible : 1;

				// 设置分歧点要求为 FFFFFFFF (不使用分歧)
				measure.Data.InNormalToAdvancedDivergePointReq = 0xFFFFFFFF;
				measure.Data.InNormalToMasterDivergePointReq = 0xFFFFFFFF;
				measure.Data.InAdvancedToMasterDivergePointReq = 0xFFFFFFFF;
				measure.Data.InAdvancedKeepAdvancedDivergePointReq = 0xFFFFFFFF;
				measure.Data.InMasterToAdvancedDivergePointReq = 0xFFFFFFFF;
				measure.Data.InMasterKeepMasterDivergePointReq = 0xFFFFFFFF;
				measure.Data._Padding2 = 0;

				// 获取该小节的滚动速度
				const ScrollChange* scrollChange = inCourse.ScrollChanges.TryFindLastAtBeat(it.Beat);
				measure.NormalNotesScrollSpeed = (scrollChange != nullptr) ? scrollChange->ScrollSpeed.GetRealPart() : 1.0f;

				measures.push_back(measure);
			}
			return ControlFlow::Fallthrough;
		});

		if (measures.empty())
		{
			// 至少需要一个小节
			Measure measure;
			measure.Data.BPM = FallbackTempo.BPM;
			measure.Data.MeasureOffset = 0.0f;
			measure.Data.IsGogoTime = 0;
			measure.Data.IsBarLineVisible = 1;
			measure.Data.InNormalToAdvancedDivergePointReq = 0xFFFFFFFF;
			measure.Data.InNormalToMasterDivergePointReq = 0xFFFFFFFF;
			measure.Data.InAdvancedToMasterDivergePointReq = 0xFFFFFFFF;
			measure.Data.InAdvancedKeepAdvancedDivergePointReq = 0xFFFFFFFF;
			measure.Data.InMasterToAdvancedDivergePointReq = 0xFFFFFFFF;
			measure.Data.InMasterKeepMasterDivergePointReq = 0xFFFFFFFF;
			measure.NormalNotesScrollSpeed = 1.0f;
			measures.push_back(measure);
		}

		// 将音符分配到对应的小节中
		for (const Note& inNote : inCourse.Notes_Normal)
		{
			// 找到该音符所属的小节
			Measure* targetMeasure = nullptr;
			size_t measureIndex = 0;
			for (size_t i = 0; i < measures.size(); ++i)
			{
				const Beat measureBeat = inCourse.TempoMap.TimeToBeat(Time::FromMS(measures[i].Data.MeasureOffset));
				const Beat nextMeasureBeat = (i + 1 < measures.size()) 
					? inCourse.TempoMap.TimeToBeat(Time::FromMS(measures[i + 1].Data.MeasureOffset))
					: maxBeat;

				if (inNote.BeatTime >= measureBeat && inNote.BeatTime < nextMeasureBeat)
				{
					targetMeasure = &measures[i];
					measureIndex = i;
					break;
				}
			}

			if (targetMeasure == nullptr)
				continue;

			NoteData fumenNote;
			fumenNote.Type = ConvertFumenNoteType(inNote.Type);
			
			if (fumenNote.Type == Fumen::FormatV2::NoteType_None)
				continue;

			// 计算音符在小节内的偏移 (毫秒)
			const Time measureTime = Time::FromMS(targetMeasure->Data.MeasureOffset);
			const Time noteTime = inCourse.TempoMap.BeatToTime(inNote.BeatTime) + inNote.TimeOffset;
			fumenNote.NoteOffset = (noteTime - measureTime).ToMS_F32();

			fumenNote._Padding1 = 0;
			fumenNote.InitialScoreValue = 0;
			fumenNote.ScoreDifferenceTimes4 = 0;
			fumenNote.BalloonHitCount_Old = 0;
			fumenNote.Length = 0.0f;

			// 处理气球和连打音符
			if (IsBalloonNote(inNote.Type))
			{
				fumenNote.InitialScoreValue = static_cast<u16>(inNote.BalloonPopCount);
				if (inNote.BeatDuration > Beat::Zero())
				{
					const Time noteDuration = inCourse.TempoMap.BeatToTime(inNote.BeatTime + inNote.BeatDuration) - inCourse.TempoMap.BeatToTime(inNote.BeatTime);
					fumenNote.Length = noteDuration.ToMS_F32();
				}
			}
			else if (IsDrumrollNote(inNote.Type))
			{
				if (inNote.BeatDuration > Beat::Zero())
				{
					const Time noteDuration = inCourse.TempoMap.BeatToTime(inNote.BeatTime + inNote.BeatDuration) - inCourse.TempoMap.BeatToTime(inNote.BeatTime);
					fumenNote.Length = noteDuration.ToMS_F32();
				}
			}

			targetMeasure->NormalNotes.push_back(fumenNote);
		}

		// 设置 NumberOfMeasures 并添加到 out
		out.ChartHeader.NumberOfMeasures = static_cast<u32>(measures.size());
		out.Measures = std::move(measures);

		return true;
	}
}
