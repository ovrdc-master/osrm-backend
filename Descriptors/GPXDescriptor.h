/*

Copyright (c) 2013, Project OSRM, Dennis Luxen, others
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef GPX_DESCRIPTOR_H
#define GPX_DESCRIPTOR_H

#include "BaseDescriptor.h"
#include "../Util/xml_renderer.hpp"

#include <osrm/json_container.hpp>

#include <iostream>

template <class DataFacadeT> class GPXDescriptor final : public BaseDescriptor<DataFacadeT>
{
  private:
    DescriptorConfig config;
    DataFacadeT *facade;

    void AddRoutePoint(const FixedPointCoordinate &coordinate, JSON::Array &json_result)
    {
        JSON::Object json_lat;
        JSON::Object json_lon;
        JSON::Array json_row;

        std::string tmp;

        FixedPointCoordinate::convertInternalLatLonToString(coordinate.lat, tmp);
        json_lat.values["_lat"] = tmp;

        FixedPointCoordinate::convertInternalLatLonToString(coordinate.lon, tmp);
        json_lon.values["_lon"] = tmp;

        json_row.values.push_back(json_lat);
        json_row.values.push_back(json_lon);
        JSON::Object entry;
        entry.values["rtept"] = json_row;
        json_result.values.push_back(entry);
    }

  public:
    explicit GPXDescriptor(DataFacadeT *facade) : facade(facade) {}

    void SetConfig(const DescriptorConfig &c) final { config = c; }

    void Run(const RawRouteData &raw_route, http::Reply &reply) final
    {
        JSON::Array json_result;
        if (raw_route.shortest_path_length != INVALID_EDGE_WEIGHT)
        {
            AddRoutePoint(raw_route.segment_end_coordinates.front().source_phantom.location,
                          json_result);

            for (const std::vector<PathData> &path_data_vector : raw_route.unpacked_path_segments)
            {
                for (const PathData &path_data : path_data_vector)
                {
                    const FixedPointCoordinate current_coordinate =
                        facade->GetCoordinateOfNode(path_data.node);
                    AddRoutePoint(current_coordinate, json_result);
                }
            }
            AddRoutePoint(raw_route.segment_end_coordinates.back().target_phantom.location,
                          json_result);
        }
        JSON::gpx_render(reply.content, json_result);
    }
};
#endif // GPX_DESCRIPTOR_H
