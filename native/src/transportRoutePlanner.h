#ifndef _OSMAND_TRANSPORT_ROUTE_PLANNER_H
#define _OSMAND_TRANSPORT_ROUTE_PLANNER_H
#include "routeCalculationProgress.h"
#include "binaryRead.h"
#include "commonOsmAndCore.h"

#include <queue>

const bool MEASURE_TIME = false;
const int64_t GEOMETRY_WAY_ID = -1;
const int64_t STOPS_WAY_ID = -2;

inline int TransportSegmentPriorityComparator(double o1DistFromStart, double o2DistFromStart) {
    if(o1DistFromStart == o2DistFromStart) {
        return 0;
    }
    return o1DistFromStart < o2DistFromStart ? -1 : 1;
}

struct TransportSegmentsComparator;
struct TransportRouteSegment;
struct TransportRoutingContext;
struct TransportRouteResult;

typedef priority_queue<SHARED_PTR<TransportRouteSegment>, vector<SHARED_PTR<TransportRouteSegment>>, TransportSegmentsComparator> TRANSPORT_SEGMENTS_QUEUE;

vector<SHARED_PTR<TransportRouteResult>> buildTransportRoute(TransportRoutingContext& ctx);
void updateCalculationProgress(SHARED_PTR<TransportRoutingContext>& ctx, TRANSPORT_SEGMENTS_QUEUE& queue);
vector<SHARED_PTR<TransportRouteResult>> prepareResults(SHARED_PTR<TransportRoutingContext>& ctx, vector<SHARED_PTR<TransportRouteSegment>>& results);
bool includeRoute(TransportRouteResult& fastRoute, TransportRouteResult& testRoute);


#endif // _OSMAND_TRANSPORT_ROUTE_PLANNER_H
