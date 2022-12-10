set(BK_GDCM_VERSION "3.1.0")
function(link_gdcm target MOD)
    target_include_directories(${target} SYSTEM ${MOD} ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/include)
    target_link_libraries(${target} ${MOD}
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmCommon.so.3.1.0
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmDICT.so.3.1.0
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmDSED.so.3.1.0
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmIOD.so.3.1.0
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmMEXD.so.3.1.0
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmMSFF.so.3.1.0
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmjpeg12.so.3.1.0
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmjpeg16.so.3.1.0
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmjpeg8.so.3.1.0
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmmd5.so.3.1.0
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmopenjp2.so.2.3.0
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmuuid.so.3.1.0
            ${PROJECT_SOURCE_DIR}/thirdparty/gdcm/lib/libgdcmzlib.so.3.1.0
            )
endfunction()
