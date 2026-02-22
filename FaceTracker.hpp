#pragma once

#include <opencv2/opencv.hpp>
#include <optional>
#include <string>
#include <thread>
#include <atomic>
#include <cstdint>
#include <vector>

/*
  Face tracking helper (header):
  - Provides a lightweight single-frame API and a background-worker mode.
  - Designed for safe lock-free publication of the latest detection using atomics.
  - The worker continuously captures frames and publishes a compact FaceResult.
*/

// A single detection result (for pull or push-style access)
struct FaceResult {
    bool        face_found = false;             // true if a face was detected in the frame
    cv::Point2f center_px{0.f, 0.f};            // face center in pixel coordinates
    cv::Point2f center_norm{0.f, 0.f};          // center normalized to [0,1] (x,y)
    float       face_size_px = 0.f;             // reported face size in pixels (max of width/height)
};

class FaceTracker {
public:
    FaceTracker() = default;
    ~FaceTracker();

    // Initialize: load Haar cascade (from resources) and open the camera device.
    // Returns true on success (cascade loaded and capture opened).
    bool init(int camera_index);

    // --- Single-thread API (compatible with a polling main loop) ---
    // Capture a single frame and run detection. Returns std::nullopt if grabbing fails.
    std::optional<FaceResult> grabAndDetect();

    // Run detection on an existing frame (no capture). Always returns a FaceResult.
    FaceResult detect(const cv::Mat& frame);

    // --- Multithreaded API (worker thread that grabs + detects continuously) ---
    // Launch a background thread that captures frames and publishes results.
    // Returns true if the worker started successfully.
    bool startWorker();           // Launch worker thread
    void stopWorker();            // Request stop and join thread

    // Poll for the latest result only if a NEW one is available since 'last_seq'.
    // On success, updates last_seq to the published sequence and returns a FaceResult.
    // Typical usage:
    //   uint64_t last_seq = 0;
    //   if (auto res = tracker.getLatest(last_seq)) { ... use *res ... }
    std::optional<FaceResult> getLatest(std::uint64_t& last_seq) const;

    // Helpers
    bool cameraOpened() const { return capture_.isOpened(); }
    bool workerRunning() const { return workerIsRunning_.load(std::memory_order_relaxed); }
    void release() { capture_.release(); }

private:
    // ---- Capture & Detector ----
    cv::VideoCapture      capture_;       // video capture device (file or camera)
    cv::CascadeClassifier faceCascade_;   // Haar cascade for face detection

    // ---- Worker Thread State ----
    std::thread       workerThread_;
    std::atomic<bool> stopRequested_{false};   // requested by main thread to stop worker
    std::atomic<bool> endOfStream_{false};     // set by worker when capture ends
    std::atomic<bool> workerIsRunning_{false}; // true while thread is active

    // ---- Published Result (lock-free reads via atomics) ----
    // These members hold the last published detection and are safe to read concurrently.
    std::atomic<bool>  lastFaceFound_{false};
    std::atomic<float> lastCenterNormX_{0.f};
    std::atomic<float> lastCenterNormY_{0.f};
    std::atomic<float> lastCenterPixX_{0.f};
    std::atomic<float> lastCenterPixY_{0.f};
    std::atomic<float> lastFaceSize_{0.f}; // published face size in px
    std::atomic<std::uint64_t> resultSequence_{0}; // incremented on each publish

    // Worker loop that continuously grabs frames and publishes results.
    void trackerThreadLoop();

    // Helpers
    // Return the largest face rectangle from a vector (or empty rect if none).
    static cv::Rect largestFace(const std::vector<cv::Rect>& faces);

    // Compute the center of the largest face in `frame`.
    // Returns true and fills out parameters if a face is found.
    bool detectFaceCenter(const cv::Mat& frame,
                          cv::Point2f& center_px,
                          cv::Point2f& center_norm,
                          float& face_size_px);
};
