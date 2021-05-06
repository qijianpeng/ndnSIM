set(name utils)
set(source_files
        batches.cpp
        dummy-keychain.cpp
        ndn-rtt-estimator.cpp
        ndn-rtt-mean-deviation.cpp
        ndn-time.cpp
        topology/annotated-topology-reader.cpp
        topology/rocketfuel-map-reader.cpp
        topology/rocketfuel-weights-reader.cpp
        )

set(header_files
        batches.hpp
        dummy-keychain.hpp
        mem-usage.hpp
        ndn-ns3-packet-tag.hpp
        ndn-rtt-estimator.hpp
        ndn-rtt-mean-deviation.hpp
        ndn-time.hpp
        topology/annotated-topology-reader.hpp
        topology/rocketfuel-map-reader.hpp
        topology/rocketfuel-weights-reader.hpp
        )

set(libraries_to_link )