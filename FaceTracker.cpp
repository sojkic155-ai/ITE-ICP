#include "FaceTracker.hpp"
#include <algorithm>   // std::max_element
#include <iostream>    // std::cerr

// Haar cascade XML should be next to your sources/executable.
static const char* kCascadeFilename = "resources/haarcascade_frontalface_default.xml";


FaceTracker::~FaceTracker() {
    // Make sure the worker is not left running.
    stopWorker();
}

bool FaceTracker::init(int camera_index) {
    // Load the cascade and open the camera input.
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


std::optional<FaceResult> FaceTracker::grabAndDetect() {
    // Grab one frame from the camera and run detection on it.
    if (!capture_.isOpened()) return std::nullopt;

    cv::Mat frame;
    if (!capture_.read(frame) || frame.empty()) {
        return std::nullopt; // camera closed or frame read failed
    }

    return detect(frame);
}

FaceResult FaceTracker::detect(const cv::Mat& frame) {
    // Detect the largest face and return its center (px + normalized).
    FaceResult result;
    if (frame.empty() || faceCascade_.empty()) {
        return result; // face_found remains false
    }

    cv::Point2f center_px, center_norm;
    float face_size_px = 0.f;
    if (detectFaceCenter(frame, center_px, center_norm, face_size_px)) {
        result.face_found  = true;
        result.center_px   = center_px;
        result.center_norm = center_norm;
        result.face_size_px = face_size_px;
    }
    return result;
}


bool FaceTracker::startWorker() {
    // Start a background thread that keeps grabbing frames and publishing results.
    if (!capture_.isOpened() || workerIsRunning_.load(std::memory_order_relaxed)) {
        return false;
    }
    stopRequested_.store(false, std::memory_order_relaxed);
    endOfStream_.store(false, std::memory_order_relaxed);
    workerIsRunning_.store(true, std::memory_order_relaxed);

    workerThread_ = std::thread(&FaceTracker::trackerThreadLoop, this);
    return true;
}

void FaceTracker::stopWorker() {
    // Ask worker to stop and wait for it (safe to call multiple times).
    if (!workerIsRunning_.load(std::memory_order_relaxed)) return;
    stopRequested_.store(true, std::memory_order_relaxed);
    if (workerThread_.joinable()) workerThread_.join();
    workerIsRunning_.store(false, std::memory_order_relaxed);
}

std::optional<FaceResult> FaceTracker::getLatest(std::uint64_t& last_seq) const {
    // Return a result only if it changed since the last poll (sequence-based).
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


void FaceTracker::trackerThreadLoop() {
    // Worker loop: read frame -> detect -> publish latest result.
    while (!stopRequested_.load(std::memory_order_relaxed)) {
        cv::Mat frame;
        if (!capture_.read(frame) || frame.empty()) {
            endOfStream_.store(true, std::memory_order_relaxed);
            break;
        }

        cv::Point2f center_px, center_norm;
        float face_size_px = 0.f;
        const bool found = detectFaceCenter(frame, center_px, center_norm, face_size_px);

        // Publish the latest result atomically
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

        // Optional throttling:
        // std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}


bool FaceTracker::detectFaceCenter(const cv::Mat& frame,
                                   cv::Point2f& center_px,
                                   cv::Point2f& center_norm,
                                   float& face_size_px) {
    // Detect faces and compute the center of the largest one.
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    // Detect faces (tune params for speed/sensitivity as needed)
    std::vector<cv::Rect> faces;
    faceCascade_.detectMultiScale(
        gray,
        faces,
        1.1,            // scaleFactor
        3,              // minNeighbors
        0,              // flags
        cv::Size(40,40) // minSize to avoid tiny false positives
    );
    if (faces.empty()) return false;

    const cv::Rect face = largestFace(faces);

    // Compute pixel center & normalized center
    center_px = {
        face.x + face.width  * 0.5f,
        face.y + face.height * 0.5f
    };
    center_norm = {
        center_px.x / static_cast<float>(frame.cols),
        center_px.y / static_cast<float>(frame.rows)
    };

    // Use maximum of width/height as a simple "size proxy" (bigger -> closer)
    face_size_px = static_cast<float>(std::max(face.width, face.height));
    return true;
}

cv::Rect FaceTracker::largestFace(const std::vector<cv::Rect>& faces) {
    // Pick the face rectangle with the biggest area.
    return *std::max_element(
        faces.begin(), faces.end(),
        [](const cv::Rect& a, const cv::Rect& b) { return a.area() < b.area(); }
    );
}
