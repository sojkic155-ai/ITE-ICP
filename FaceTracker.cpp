#include "FaceTracker.hpp"
#include <algorithm>   // std::max_element
#include <iostream>    // std::cerr

// Haar cascade XML should be next to your sources/executable.
static const char* kCascadeFilename = "resources/haarcascade_frontalface_default.xml";


// Destructor: ensure the background worker is stopped before destruction.
FaceTracker::~FaceTracker() {
    stopWorker();
}

// Initialize detector and open the capture device.
// Returns false if cascade fails to load or camera cannot be opened.
bool FaceTracker::init(int camera_index) {
    if (!faceCascade_.load(kCascadeFilename)) {
        std::cerr << "Failed to load cascade: " << kCascadeFilename << '\n';
        return false;
    }
    if (!capture_.open(camera_index)) {
        std::cerr << "Failed to open camera index " << camera_index << '\n';
        return false;
    }
    return true;
}


// Single-frame helper: grab one frame and run detection.
// Returns std::nullopt on capture failure / closed device.
std::optional<FaceResult> FaceTracker::grabAndDetect() {
    if (!capture_.isOpened()) return std::nullopt;

    cv::Mat frame;
    if (!capture_.read(frame) || frame.empty()) {
        return std::nullopt; // camera closed or frame read failed
    }

    return detect(frame);
}

// Run detection on the provided frame and return a FaceResult.
// Non-throwing and returns result.face_found == false when no detection.
FaceResult FaceTracker::detect(const cv::Mat& frame) {
    FaceResult result;
    if (frame.empty() || faceCascade_.empty()) {
        return result; // early-out: no data or detector not ready
    }

    cv::Point2f center_px, center_norm;
    float face_size_px = 0.f;
    if (detectFaceCenter(frame, center_px, center_norm, face_size_px)) {
        result.face_found   = true;
        result.center_px    = center_px;
        result.center_norm  = center_norm;
        result.face_size_px = face_size_px;
    }
    return result;
}


// Start background worker thread that continuously captures frames and publishes results.
// Returns false if capture isn't open or worker is already running.
bool FaceTracker::startWorker() {
    if (!capture_.isOpened() || workerIsRunning_.load(std::memory_order_relaxed)) {
        return false;
    }
    stopRequested_.store(false, std::memory_order_relaxed);
    endOfStream_.store(false, std::memory_order_relaxed);
    workerIsRunning_.store(true, std::memory_order_relaxed);

    workerThread_ = std::thread(&FaceTracker::trackerThreadLoop, this);
    return true;
}

// Request worker stop and join the thread. Safe to call repeatedly.
void FaceTracker::stopWorker() {
    if (!workerIsRunning_.load(std::memory_order_relaxed)) return;
    stopRequested_.store(true, std::memory_order_relaxed);
    if (workerThread_.joinable()) workerThread_.join();
    workerIsRunning_.store(false, std::memory_order_relaxed);
}

// Return the last published result only when the sequence changed since last_seq.
// On success updates last_seq and returns FaceResult; otherwise returns std::nullopt.
std::optional<FaceResult> FaceTracker::getLatest(std::uint64_t& last_seq) const {
    const auto current_seq = resultSequence_.load(std::memory_order_relaxed);
    if (current_seq == last_seq) {
        return std::nullopt; // no new result since last poll
    }
    last_seq = current_seq;

    FaceResult r;
    r.face_found = lastFaceFound_.load(std::memory_order_relaxed);
    if (r.face_found) {
        r.center_norm = {
            lastCenterNormX_.load(std::memory_order_relaxed),
            lastCenterNormY_.load(std::memory_order_relaxed)
        };
        r.center_px = {
            lastCenterPixX_.load(std::memory_order_relaxed),
            lastCenterPixY_.load(std::memory_order_relaxed)
        };
        r.face_size_px = lastFaceSize_.load(std::memory_order_relaxed);
    }
    return r;
}


// Worker main loop: capture, detect largest face, and publish compact result via atomics.
// Uses relaxed atomics for low-overhead publication; consumer must tolerate eventual consistency.
void FaceTracker::trackerThreadLoop() {
    while (!stopRequested_.load(std::memory_order_relaxed)) {
        cv::Mat frame;
        if (!capture_.read(frame) || frame.empty()) {
            endOfStream_.store(true, std::memory_order_relaxed);
            break;
        }

        cv::Point2f center_px, center_norm;
        float face_size_px = 0.f;
        const bool found = detectFaceCenter(frame, center_px, center_norm, face_size_px);

        // Publish the latest result atomically.
        lastFaceFound_.store(found, std::memory_order_relaxed);
        if (found) {
            lastCenterPixX_.store(center_px.x, std::memory_order_relaxed);
            lastCenterPixY_.store(center_px.y, std::memory_order_relaxed);
            lastCenterNormX_.store(center_norm.x, std::memory_order_relaxed);
            lastCenterNormY_.store(center_norm.y, std::memory_order_relaxed);
            lastFaceSize_.store(face_size_px, std::memory_order_relaxed);
        } else {
            lastFaceSize_.store(0.f, std::memory_order_relaxed);
        }
        resultSequence_.fetch_add(1, std::memory_order_relaxed);

        // Optional throttle to reduce CPU usage when a very fast camera is present.
        // std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}


// Detect faces in a frame and compute center + normalized center for the largest face.
// Returns true if a face was found and fills out the out-params.
bool FaceTracker::detectFaceCenter(const cv::Mat& frame,
                                   cv::Point2f& center_px,
                                   cv::Point2f& center_norm,
                                   float& face_size_px) {
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    // Detect faces with modest minimum size to reduce false positives.
    std::vector<cv::Rect> faces;
    faceCascade_.detectMultiScale(
        gray,
        faces,
        1.1,            // scaleFactor
        3,              // minNeighbors
        0,              // flags
        cv::Size(40,40) // minSize
    );
    if (faces.empty()) return false;

    const cv::Rect face = largestFace(faces);

    // Pixel center and normalized center in [0,1].
    center_px = {
        face.x + face.width  * 0.5f,
        face.y + face.height * 0.5f
    };
    center_norm = {
        center_px.x / static_cast<float>(frame.cols),
        center_px.y / static_cast<float>(frame.rows)
    };

    // Use maximum of width/height as a simple proxy for distance/size.
    face_size_px = static_cast<float>(std::max(face.width, face.height));
    return true;
}

// Return the face rectangle with the largest area (assumes non-empty input).
cv::Rect FaceTracker::largestFace(const std::vector<cv::Rect>& faces) {
    return *std::max_element(
        faces.begin(), faces.end(),
        [](const cv::Rect& a, const cv::Rect& b) { return a.area() < b.area(); }
    );
}
