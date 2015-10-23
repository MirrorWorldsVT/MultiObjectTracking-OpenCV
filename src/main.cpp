#include <cstdlib>
#include <string>
#include <memory>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include "camera.h"
#include "display.h"
#include "track.h"
#include "objecttracker.h"
#include "trackers/difference.h"
#include "Networking/blob.h"
#include "Networking/blobSender.h"

const std::string serverURL = "dev.mirrorworlds.icat.vt.edu";
const int serverPort        = 9999;

void sendTracks(int cameraId, ObjectTracker* tracker, blobSender& sender) {
    Blob blobData;
    memset(&blobData, 0, sizeof(blobData));
    blobData.cameraID = cameraId;

    // Send deleted blobs first
    for(int id : tracker->getDeletedTracks()) {
        blobData.id = id;
        sender.sendRemoveBlob(&blobData);
    }

    // Then send new/updated blobs
    for(const std::unique_ptr<Track>& track : tracker->getTracks()) {
        blobData.id = track->getId();
        const cv::Rect& bbox = track->getBBox();
        blobData.bounding_x = bbox.x;
        blobData.bounding_y = bbox.y;
        blobData.bounding_width = bbox.width;
        blobData.bounding_height = bbox.height;
        blobData.origin_x = (bbox.x + bbox.width) /2.0;
        blobData.origin_y = (bbox.y + bbox.height) /2.0;

        if(track->getAge() == 1) {
            sender.sendNewBlob(&blobData);
        } else {
            sender.sendUpdateBlob(&blobData);
        }
    }
}

void printUsage(std::string progName) {
    std::cout << "Usage: " << progName << " <id> <ip or url or file>" << std::endl;
}

std::string parseURL(std::string arg) {
    in_addr ipaddr;
    if(inet_pton(AF_INET, arg.c_str(), &ipaddr) == 1) {
        std::cout << "Using camera at address " << arg << std::endl;
        return std::string("http://admin:admin@") + arg + std::string("/video.cgi?.mjpg");
    } else {
        std::cout << "Using file/URL " << arg << std::endl;
        return arg;
    }
}

int main(int argc, char *argv[]) {
    int id;
    std::string url;
    // Read arguments and set parameters
    if(argc == 1) {
        // Defaults
        std::cout << "Using default video input" << std::endl;
        id = 0;
        url = "../walk-cut.mov";
    } else if(argc != 3 || (argc == 2 && std::string(argv[1]) == "--help")) {
        printUsage(argv[0]);
        return 0;
    } else {
        id = std::atoi(argv[1]);
        url = parseURL(argv[2]);
    }

    // Initialize system objects
    Camera camera(id, url);
    Display display(id);
    ObjectTracker* tracker = new DifferenceTracker();
    blobSender sender(serverURL.c_str(), serverPort); // set up networking

    // Start video processing
    try {
        cv::UMat frame;
        while(camera.getVideo().read(frame)) {
            tracker->processFrame(frame);
            display.showFrame(frame, tracker->getMaskImage(), tracker->getTracks());
            sendTracks(id, tracker, sender);
            // Only the least-signficant byte is used, sometimes the rest is garbage so 0xFF is needed
            int key = cv::waitKey(10) & 0xFF;
            if(key == 27) { // Escape pressed
                break;
            }
        }
    } catch(const std::exception& ex) {
        std::cerr << "Error occurred: " << ex.what() << std::endl;
    }
}
