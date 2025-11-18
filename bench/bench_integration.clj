(ns bench-integration
  (:require
   [clojure.edn :as clojure-edn]
   [clojure.java.io :as io]
   [criterium.core :refer [quick-benchmark]]
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

(defn bench-parse [parser-fn data size]
  (let [results (quick-benchmark (parser-fn data) {})
        mean-s (first (:mean results))
        mean-us (* mean-s 1e6)
        stddev-us (* (Math/sqrt (first (:variance results))) 1e6)
        sample-count (:sample-count results)
        execution-count (:execution-count results)
        total-samples (* sample-count execution-count)
        ;; Calculate throughput
        throughput-gbps (/ size mean-s 1024.0 1024.0 1024.0)]

    {:iterations total-samples
     :total-ms (* (:total-time results) 1e3)
     :mean-us mean-us
     :stddev-us stddev-us
     :throughput-gbps throughput-gbps
     :size size}))

(defn print-header []
  (println (format "%-25s %14s  %10s  %20s  %10s  %s"
                   "Benchmark" "Iterations" "Total (ms)" "Mean (μs)" "Throughput" "Size"))
  (println (format "%-25s %14s  %10s  %20s  %10s  %s"
                   "---------" "----------" "----------" "---------" "----------" "----")))

(defn print-result [description result]
  (printf "%-25s %,14d  %10.2f  %10.3f ± %-7.3f  %5.3f GB/s  (%d bytes)\n"
          description
          (:iterations result)
          (:total-ms result)
          (:mean-us result)
          (:stddev-us result)
          (:throughput-gbps result)
          (:size result))
  (flush))

(defn bench-file [parser-fn filename description]
  (let [path (str "bench/data/" filename)
        data (read-file-str path)
        size (file-size path)
        full-desc (str description " (" (format-size size) ")")]
    (try
      (let [result (bench-parse parser-fn data size)]
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
  (bench-file parser-fn "basic_10.edn" "basic_10")
  (bench-file parser-fn "basic_100.edn" "basic_100")
  (bench-file parser-fn "basic_1000.edn" "basic_1000")
  (bench-file parser-fn "basic_10000.edn" "basic_10000")
  (bench-file parser-fn "basic_100000.edn" "basic_100000")
  
  ;; Keywords
  (println "\n--- Keyword Vectors ---")
  (bench-file parser-fn "keywords_10.edn" "keywords_10")
  (bench-file parser-fn "keywords_100.edn" "keywords_100")
  (bench-file parser-fn "keywords_1000.edn" "keywords_1000")
  (bench-file parser-fn "keywords_10000.edn" "keywords_10000")
  
  ;; Integers
  (println "\n--- Integer Arrays ---")
  (bench-file parser-fn "ints_1400.edn" "ints_1400")
  
  ;; Strings
  (println "\n--- String Collections ---")
  (bench-file parser-fn "strings_1000.edn" "strings_1000")
  (bench-file parser-fn "strings_uni_250.edn" "strings_uni_250")
  
  ;; Nested
  (println "\n--- Nested Structures ---")
  (bench-file parser-fn "nested_100000.edn" "nested_100000")
  
  (println)
  (println "Notes:")
  (println "  - GB/s calculated as: (iterations × file_size) / time / 1024³"))

(defn -main [& _]
  ;; Run clojure.edn benchmarks
  (run-benchmarks "clojure.edn" clojure-edn/read-string)
  
  (println "\n" (apply str (repeat 80 "=")))
  
  ;; Run fast-edn benchmarks
  (run-benchmarks "fast-edn" fast-edn/read-string)
  
  (shutdown-agents))
