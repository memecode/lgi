
set(CODE_SIGN_IDENTITY "fret@memecode.com") # Replace with your code signing identity

# Scan all the deps and copy them into the app bundle:
function(macCopyDeps topTarget target)

    # Process all the target's dependencies:
    get_target_property(LIBS ${target} LINK_LIBRARIES)
    # message("Deps for ${target} = ${LIBS}")
    foreach(LIB ${LIBS})
        if (TARGET ${LIB}) # We only look at Cmake targets at the moment
            get_target_property(LIB_FRAMEWORK ${LIB} FRAMEWORK)
            get_target_property(LIB_TYPE ${LIB} TYPE)
            if (${LIB_FRAMEWORK})
                
                # copy the whole framework folder to the app bundle
                add_custom_command(
                    TARGET ${topTarget} POST_BUILD 
                    COMMAND "${CMAKE_COMMAND}" -E echo "Copy framework ${LIB} to ${target} Frameworks folder"
                    COMMAND rm -rf "$<TARGET_BUNDLE_CONTENT_DIR:${topTarget}>/Frameworks/$<TARGET_BUNDLE_DIR_NAME:${LIB}>"
                    COMMAND cp -R
                            "$<TARGET_BUNDLE_DIR:${LIB}>"
                            "$<TARGET_BUNDLE_CONTENT_DIR:${topTarget}>/Frameworks/$<TARGET_BUNDLE_DIR_NAME:${LIB}>"
                    COMMAND codesign -f -s ${CODE_SIGN_IDENTITY} "$<TARGET_BUNDLE_CONTENT_DIR:${topTarget}>/Frameworks/$<TARGET_BUNDLE_DIR_NAME:${LIB}>/Versions/A"
                    )
    
            elseif (LIB_TYPE MATCHES "SHARED")
                
                # Copy over the shared object (dylib)
                add_custom_command(
                    TARGET ${topTarget} POST_BUILD 
                    COMMAND "${CMAKE_COMMAND}" -E echo "Copy dylib ${LIB} to ${target} Frameworks folder"
                    COMMAND "${CMAKE_COMMAND}" -E copy
                        "$<TARGET_FILE:${LIB}>"
                        "$<TARGET_BUNDLE_CONTENT_DIR:${topTarget}>/Frameworks"
                    COMMAND codesign -f -s ${CODE_SIGN_IDENTITY} "$<TARGET_BUNDLE_CONTENT_DIR:${topTarget}>/Frameworks/$<TARGET_FILE_NAME:${LIB}>"
                    )

                # Also do child deps:
                macCopyDeps(${topTarget} ${LIB})
            
            endif()
        elseif (NOT ${LIB} MATCHES "\.tbd")
            # message("Warning: ${LIB} is not a target.")
        endif()
    endforeach()

endfunction()

# Run this on your app bundle target after everything is done.
# It'll copy in all the depend libs and frameworks, and then fix up 
# the install paths
#
# Requires 'python3' to run fixInstallPaths.py. That could be implemented
# in cmake, but it'd be 3 times as long and right now I don't wanna.
function(macBundlePostBuild target)

    # Create the frameworks folder
    add_custom_command(
        TARGET ${target} POST_BUILD 
        COMMAND "${CMAKE_COMMAND}" -E echo "Make ${target} Frameworks folder"
        COMMAND "${CMAKE_COMMAND}" -E make_directory 
                "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks")

    # Recurse through deps and copy them all:
    macCopyDeps(${target} ${target})

    # Then fixup all the paths:
    add_custom_command(
        TARGET ${target} POST_BUILD 
        COMMAND "${CMAKE_COMMAND}" -E echo "Fixing ${target} install paths..."
        COMMAND "${LGI_DIR}/src/mac/fixInstallPaths.py" "$<TARGET_FILE:${target}>")

    # Finally code sign the application binary again (we broke it fixing the install paths):
    add_custom_command(
        TARGET ${target} POST_BUILD 
        COMMAND "${CMAKE_COMMAND}" -E echo "Signing the ${target} app bundle..."
        COMMAND codesign -f -s ${CODE_SIGN_IDENTITY} "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Macos/$<TARGET_FILE_NAME:${target}>"
        )
endfunction()