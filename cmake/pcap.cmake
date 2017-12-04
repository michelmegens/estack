IF(CMAKE_BUILD_TYPE MATCHES Debug)
IF(WIN32)
	link_directories(${PROJECT_SOURCE_DIR}/external/pcap/lib)
	include_directories(${PROJECT_SOURCE_DIR}/external/pcap/include)
	SET(PCAP_LIBRARY wpcap packet)
ELSE()
	find_library(PCAP REQUIRED)
	SET(PCAP_LIBRARY pcap)
ENDIF()
ENDIF()