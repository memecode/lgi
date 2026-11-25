#pragma once

#include "lgi/common/CurrentTime.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

inline bool GraphProfileEnabled()
{
	static const bool enabled = std::getenv("LVC_TRACE_GRAPH") != nullptr;
	return enabled;
}

inline uint64_t GraphProfileTime()
{
	return GraphProfileEnabled() ? LMicroTime() : 0;
}

inline void TraceGraph(const char *event, const char *folder, const char *format, ...)
{
	if (!GraphProfileEnabled())
		return;

	std::fprintf(stderr, "[lvc-graph] %s folder='%s' ", event, folder ? folder : "");
	va_list args;
	va_start(args, format);
	std::vfprintf(stderr, format, args);
	va_end(args);
	std::fputc('\n', stderr);
}

struct GraphPaintProfile
{
	inline static thread_local GraphPaintProfile *Current = nullptr;
	GraphPaintProfile *Previous;
	LString Folder;
	uint64_t Started;
	uint64_t Base = 0, Setup = 0, Edges = 0, Nodes = 0, MaxRow = 0;
	size_t Rows = 0, Positions = 0, Segments = 0, Curves = 0, NoVector = 0;

	GraphPaintProfile() : Previous(Current), Started(GraphProfileTime())
	{
		if (GraphProfileEnabled())
			Current = this;
	}

	~GraphPaintProfile()
	{
		Current = Previous;
		if (!Rows)
			return;

		auto elapsed = GraphProfileTime() - Started;
		TraceGraph("Paint", Folder.Get(),
			"list_ms=%.3f graph_ms=%.3f base_ms=%.3f setup_ms=%.3f edges_ms=%.3f nodes_ms=%.3f "
			"max_row_ms=%.3f rows=%zu positions=%zu segments=%zu curves=%zu no_vector=%zu",
			elapsed / 1000.0, (Base + Setup + Edges + Nodes) / 1000.0,
			Base / 1000.0, Setup / 1000.0, Edges / 1000.0, Nodes / 1000.0,
			MaxRow / 1000.0, Rows, Positions, Segments, Curves, NoVector);
	}
};
