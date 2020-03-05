class BoostAT166 < Formula
  desc "Collection of portable C++ source libraries"
  homepage "https://www.boost.org/"
  url "https://dl.bintray.com/boostorg/release/1.66.0/source/boost_1_66_0.tar.bz2"
  sha256 "5721818253e6a0989583192f96782c4a98eb6204965316df9f5ad75819225ca9"
  head "https://github.com/boostorg/boost.git"

  bottle do
    cellar :any
    sha256 "78cb090c515e20aa7307c6619a055ffd8858cc6a3bd756958edbf34f463e4bc1" => :high_sierra
    sha256 "51e3b67625b8def53f5e5183a5ef98071e17b0b760dfc6baa65877d4528141ca" => :sierra
    sha256 "6352d4f65d7595c1eff62c6ce07588944122fa3305a77813fdbc16747b7dddce" => :el_capitan
  end

  option "with-icu4c", "Build regexp engine with icu support"
  option "without-single", "Disable building single-threading variant"
  option "without-static", "Disable building static library variant"

  deprecated_option "with-icu" => "with-icu4c"

  depends_on "icu4c" => :optional

  # Fix <experimental/string_view> for libc++7.0+
  # https://github.com/boostorg/asio/pull/91
  patch :DATA

  def install
    # Force boost to compile with the desired compiler
    open("user-config.jam", "a") do |file|
      file.write "using darwin : : #{ENV.cxx} ;\n"
    end

    # libdir should be set by --prefix but isn't
    bootstrap_args = ["--prefix=#{prefix}", "--libdir=#{lib}"]

    if build.with? "icu4c"
      icu4c_prefix = Formula["icu4c"].opt_prefix
      bootstrap_args << "--with-icu=#{icu4c_prefix}"
    else
      bootstrap_args << "--without-icu"
    end

    # Handle libraries that will not be built.
    without_libraries = ["python", "mpi"]

    # Boost.Log cannot be built using Apple GCC at the moment. Disabled
    # on such systems.
    without_libraries << "log" if ENV.compiler == :gcc

    bootstrap_args << "--without-libraries=#{without_libraries.join(",")}"

    # layout should be synchronized with boost-python and boost-mpi
    args = ["--prefix=#{prefix}",
            "--libdir=#{lib}",
            "-d2",
            "-j#{ENV.make_jobs}",
            "--layout=tagged",
            "--user-config=user-config.jam",
            "-sNO_LZMA=1",
            "install"]

    if build.with? "single"
      args << "threading=multi,single"
    else
      args << "threading=multi"
    end

    if build.with? "static"
      args << "link=shared,static"
    else
      args << "link=shared"
    end

    # Trunk starts using "clang++ -x c" to select C compiler which breaks C++11
    # handling using ENV.cxx11. Using "cxxflags" and "linkflags" still works.
    args << "cxxflags=-std=c++11"
    if ENV.compiler == :clang
      args << "cxxflags=-stdlib=libc++" << "linkflags=-stdlib=libc++"
    end

    system "./bootstrap.sh", *bootstrap_args
    system "./b2", "headers"
    system "./b2", *args
  end

  def caveats
    s = ""
    # ENV.compiler doesn't exist in caveats. Check library availability
    # instead.
    if Dir["#{lib}/libboost_log*"].empty?
      s += <<~EOS
        Building of Boost.Log is disabled because it requires newer GCC or Clang.
      EOS
    end

    s
  end

  test do
    (testpath/"test.cpp").write <<~EOS
      #include <boost/algorithm/string.hpp>
      #include <string>
      #include <vector>
      #include <assert.h>
      using namespace boost::algorithm;
      using namespace std;

      int main()
      {
        string str("a,b");
        vector<string> strVec;
        split(strVec, str, is_any_of(","));
        assert(strVec.size()==2);
        assert(strVec[0]=="a");
        assert(strVec[1]=="b");
        return 0;
      }
    EOS
    system ENV.cxx, "test.cpp", "-std=c++1y", "-L#{lib}", "-lboost_system", "-o", "test"
    system "./test"
  end
end

__END__
diff -Nur boost_1_66_0/boost/asio/detail/config.hpp boost_1_66_0-patched/boost/asio/detail/config.hpp
--- boost_1_66_0/boost/asio/detail/config.hpp   2017-12-06 00:00:00.000000000 -0700
+++ boost_1_66_0-patched/boost/asio/detail/config.hpp   2020-03-05 00:00:00.000000000 -0800
@@ -769,14 +769,21 @@
 #endif // !defined(BOOST_ASIO_HAS_STD_FUTURE)

 // Standard library support for experimental::string_view.
+// Note: libc++ 7.0 keeps <experimental/string_view> header but deprecates it
+// with an #error directive.
 #if !defined(BOOST_ASIO_HAS_STD_STRING_VIEW)
 # if !defined(BOOST_ASIO_DISABLE_STD_STRING_VIEW)
 #  if defined(__clang__)
 #   if (__cplusplus >= 201402)
 #    if __has_include(<experimental/string_view>)
-#     define BOOST_ASIO_HAS_STD_STRING_VIEW 1
-#     define BOOST_ASIO_HAS_STD_EXPERIMENTAL_STRING_VIEW 1
+#     if defined(_LIBCPP_LFTS_STRING_VIEW)
+#      define BOOST_ASIO_HAS_STD_EXPERIMENTAL_STRING_VIEW 1
+#      define BOOST_ASIO_HAS_STD_STRING_VIEW 1
+#     endif // defined(_LIBCPP_LFTS_STRING_VIEW)
 #    endif // __has_include(<experimental/string_view>)
+#    if __has_include(<string_view>)
+#     define BOOST_ASIO_HAS_STD_STRING_VIEW 1
+#    endif // __has_include(<string_view>)
 #   endif // (__cplusplus >= 201402)
 #  endif // defined(__clang__)
 #  if defined(__GNUC__)
