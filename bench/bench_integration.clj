(ns bench-integration
  (:require
   [clojure.edn :as clojure-edn]
   [clojure.java.io :as io]
   [fast-edn.core :as fast-edn]))

(defn read-file-str [path]
  (slurp path))

(defn file-size [path]
  (.length (io/file path)))

(defn format-size [bytes]
  (cond
    (>= bytes (* 1024 1024)) (format "%.0f KB" (/ bytes 1024.0))
    (>= bytes 1024) (format "%.1f KB" (/ bytes 1024.0))
    :else (format "%d bytes" bytes)))

(defn bench-parse [parser-fn data size _]
  (let [warmup-iterations 3
        min-duration-ms 500
        min-iterations 1000]
    
    ;; Warmup
    (dotimes [_ warmup-iterations]
      (parser-fn data))
    
    ;; Benchmark loop
    (let [start-time (System/nanoTime)
          target-ns (* min-duration-ms 1000000)]
      (loop [iterations 0
             elapsed 0]
        (if (and (>= elapsed target-ns) (>= iterations min-iterations))
          ;; Done - calculate results
          (let [mean-us (/ elapsed iterations 1000.0)
                total-ms (/ elapsed 1000000.0)
                total-bytes (* iterations size)
                time-seconds (/ elapsed 1e9)
                throughput-gbps (/ total-bytes time-seconds 1024.0 1024.0 1024.0)]
            {:iterations iterations
             :total-ms total-ms
             :mean-us mean-us
             :throughput-gbps throughput-gbps
             :size size})
          ;; Continue benchmarking
          (do
            (parser-fn data)
            (let [now (System/nanoTime)
                  new-elapsed (- now start-time)]
              (recur (inc iterations) new-elapsed))))))))

(defn print-header []
  (println (format "%-40s %14s  %8s  %10s  %10s  %s"
                   "Benchmark" "Iterations" "Total" "Mean" "Throughput" "Size"))
  (println (format "%-40s %14s  %8s  %10s  %10s  %s"
                   "---------" "----------" "-----" "----" "----------" "----")))

(defn print-result [description result]
  (printf "%-40s %,14d  %8.2f  %10.3f  %10.3f  (%d bytes)\n"
          description
          (:iterations result)
          (:total-ms result)
          (:mean-us result)
          (:throughput-gbps result)
          (:size result))
  (flush))

(defn bench-file [parser-fn filename description]
  (let [path (str "bench/data/" filename)
        data (read-file-str path)
        size (file-size path)
        full-desc (str description)]
    (try
      (let [result (bench-parse parser-fn data size full-desc)]
        (print-result full-desc result))
      (catch Exception e
        (printf "%-40s FAILED (%s)\n" full-desc (.getMessage e))
        (flush)))))

(defn run-benchmarks [parser-name parser-fn]
  (println (format "\n%s Integration Benchmarks" parser-name))
  (println "============================\n")
  
  (print-header)
  
  ;; Basic maps
  (println "\n--- Basic Maps ---")
  (bench-file parser-fn "basic_10.edn" "basic_10 (9 bytes)")
  (bench-file parser-fn "basic_100.edn" "basic_100 (97 bytes)")
  (bench-file parser-fn "basic_1000.edn" "basic_1000 (898 bytes)")
  (bench-file parser-fn "basic_10000.edn" "basic_10000 (10 KB)")
  (bench-file parser-fn "basic_100000.edn" "basic_100000 (99 KB)")
  
  ;; Keywords
  (println "\n--- Keyword Vectors ---")
  (bench-file parser-fn "keywords_10.edn" "keywords_10 (116 bytes)")
  (bench-file parser-fn "keywords_100.edn" "keywords_100 (886 bytes)")
  (bench-file parser-fn "keywords_1000.edn" "keywords_1000 (9.7 KB)")
  (bench-file parser-fn "keywords_10000.edn" "keywords_10000 (117 KB)")
  
  ;; Integers
  (println "\n--- Integer Arrays ---")
  (bench-file parser-fn "ints_1400.edn" "ints_1400 (10 KB)")
  
  ;; Strings
  (println "\n--- String Collections ---")
  (bench-file parser-fn "strings_1000.edn" "strings_1000 (55 KB)")
  (bench-file parser-fn "strings_uni_250.edn" "strings_uni_250 (56 KB)")
  
  ;; Nested
  (println "\n--- Nested Structures ---")
  (bench-file parser-fn "nested_100000.edn" "nested_100000 (96 KB)")
  
  (println)
  (println "Notes:")
  (println "  - Throughput includes only parsing (no explicit freeing)")
  (println "  - Each benchmark runs for minimum 500ms or 1000 iterations")
  (println "  - Warmup: 3 iterations before measurement")
  (println "  - GB/s calculated as: (iterations × file_size) / time / 1024³"))

(defn -main [& _]
  ;; Run clojure.edn benchmarks
  (run-benchmarks "clojure.edn" clojure-edn/read-string)
  
  (println "\n" (apply str (repeat 80 "=")))
  
  ;; Run fast-edn benchmarks
  (run-benchmarks "fast-edn" fast-edn/read-string)
  
  (shutdown-agents))
