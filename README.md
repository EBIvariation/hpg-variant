
# HPG VARIANT

HPG Variant is a suite for the analysis of genomic data extracted from Next Generation Sequencing technologies.
It uses parallel computing technologies such as OpenMP in order to reduce the processing times.

It contains three binaries:
* hpg-var-effect retrieves the effect of genome variations.
* hpg-var-gwas conducts genomic-wide association analysis such as chi-square and Fisher's exact test, and family-based analysis such as transmission disequilibrium test (TDT).,
* hpg-var-vcf allows to preprocess files containing genome variations in Variant Call Format. Filtering, merging, splitting and retrieving statistics are the features currently implemented.


## CONTENTS
1. Compilation steps in Debian / Ubuntu
2. Compilation steps in Fedora
3. Command-line autocompletion
4. Obtaining unique set of chromosomes


## COMPILATION STEPS IN DEBIAN / UBUNTU

If you have downloaded HPG Variant as a .deb package file, just run 'sudo dkpg -i hpg-variant_X.Y-amd64.deb', and don't forget to replace X.Y with the corresponding version number of your package.
In case you downloaded a tarball with the source code, please follow these instructions:

### PRE-BUILD
You need a C compiler, the build system SCons and a set of libraries that can be downloaded using apt-get or aptitude.
sudo apt-get install build-essential scons libcurl4-openssl-dev libgsl0-dev libxml2-dev zlib1g-dev libncurses5-dev

### BUILD
From the root folder of the application, type:
* 'git submodule update --init' to initialize the submodules with libraries' code
* 'scons' to compile HPG Variant. This will create 3 binaries in the 'bin' subfolder.
If you have a computer with more than one core, you can use the -j parameter to speed-up the compilation, like in 'scons -j4'. This is the typical situation for most modern machines.

### RUN
If you want to execute HPG Variant (not just build it), you need to run the following command:
sudo apt-get install libcurl4-openssl-dev libgsl0ldbl libxml2 zlib1g

### INSTALL
This step is not neccessary to run HPG Variant, but to install it in your system.
Type 'scons debian' to create a file named 'hpg-variant_X.Y-amd64.deb' (with X.Y being the version number) in the root folder of HPG Variant.
Type 'sudo dkpg -i hpg-variant_X.Y-amd64.deb', and don't forget to replace X.Y with the corresponding version number of your package.



## COMPILATION STEPS IN FEDORA

If you have downloaded HPG Variant as a .rpm package file, just run 'sudo rpm -i hpg-variant_X.Y.fc17.x86_64.rpm', and don't forget to replace X.Y with the corresponding version number of your package.
In case you downloaded a tarball with the source code, please follow these instructions:

### PRE-BUILD
You need a C compiler, the build system SCons and a set of libraries that can be downloaded using yum.
sudo yum install gcc-c++ glibc-devel scons libcurl-devel gsl-devel libxml2-devel zlib-devel ncurses-devel

### BUILD
From the root folder of the application, type:
* 'git submodule update --init' to initialize the submodules with libraries' code
* 'scons' to compile HPG Variant. This will create 3 binaries in the 'bin' subfolder.
If you have a computer with more than one core, you can use the -j parameter to speed-up the compilation, like in 'scons -j4'. This is the typical situation for most modern machines.

### RUN
If you want to execute HPG Variant (not just build it), you need to run the following command:
sudo yum install libcurl gsl libxml2 zlib

### INSTALL
This step is not neccessary to run HPG Variant, but to install it in your system.
In order to create RPM packages, you must install the rpmbuild tool by running the command 'sudo yum install rpm-build'.

1) You need a tarball, so run 'scons tarball' from the root of the hpg-variant folder.
2) Copy this tarball to the ~/rpmbuild/SOURCES folder.
3) Copy the file rpm/hpg-variant.spec to the ~/rpmbuild/SPECS folder.
4) From the ~/rpmbuild/SPECS folder, run 'rpmbuild -ba hpg-variant.spec'.
4) The RPM package should be generated. In ~/rpmbuild/RPMS/x86_64 you should find a file named 'hpg-variant_X.Y.fc17.x86_64.rpm' (with X.Y being the version number), and another one in the ~/rpmbuild/SRPMS folder.
5) Type 'sudo rpm -i hpg-variant_X.Y.fc17.x86_64.rpm', and don't forget to replace X.Y with the corresponding version number of your package.


## COMMAND-LINE AUTOCOMPLETION

HPG Variant supports command-line autocompletion in Bash shells, but it has to be manually enabled or the machine rebooted.

- If you installed a .deb/.rpm package, there should be 3 files in the '/etc/bash_completion.d' directory, with the names 'hpg-var-effect', 'hpg-var-gwas' and 'hpg-var-vcf'. Type '. /etc/bash_autocompletion' in your Bash shell and it will be turned on.
- If you compile HPG Variant, the 'bin/bash_completion' subfolder should contain the same 3 files. Please copy them to the '/etc/bash_completion.d' directory (don't forget you need to be root) and type '. /etc/bash_autocompletion' in order to turn it on.


## OBTAINING UNIQUE SET OF CHROMOSOMES

HPG Variant needs to know how chromosomes are sorted across all the input files. For non-trivial cases (e.g. high number of files with many scaffolds) a 'toposorter' tool is provided in the 'utils' folder.
Dependencies are listed in the 'requirements.txt' file and need to be installed before running said tool.
