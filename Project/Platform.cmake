if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_CXX_FLAGS "-x objective-c++ ${CMAKE_CXX_FLAGS}")
    if(IOS)
        add_definitions(-DK3DPLATFORM_OS_IOS=1)
    else()
        set(MACOS TRUE)
        add_definitions(-DK3DPLATFORM_OS_MAC=1)

		set(MACOSX_BUNDLE_INFO_STRING "${PROJECT_NAME}")
		set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.tsinstudio.kaleido3d")
		set(MACOSX_BUNDLE_COPYRIGHT "Copyright Tsin Studio 2016. All Rights Reserved.")
    endif()
endif()
