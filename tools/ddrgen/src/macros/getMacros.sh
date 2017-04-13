#! /bin/bash

###############################################################################
#
# (c) Copyright IBM Corp. 2016
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################

##
# Create a list of all cpp, hpp, c, and h files that have a simple (non function) macro within them.
##
get_all_annotated_files() {
	local annotated_files=$(grep -rlE "@ddr_(namespace|options): " ${scan_dir} | grep -e '\.[ch]pp$' -e '\.[ch]$')
	echo "${annotated_files[@]}"
}

get_ddr_options() {
	# Check the file for "@ddr_options" or use the default settings.
	if grep -qE "@ddr_options: valuesonly" ${f}; then
		addValues=1
		addFlags=0
	elif grep -qE "@ddr_options: buildflagsonly" ${f}; then
		addValues=0
		addFlags=1
	elif grep -qE "@ddr_options: valuesandbuildflags" ${f}; then
		addValues=1
		addFlags=1
	elif grep -qE "@ddr_namespace: (default|map_to_type=.*)" ${f}; then
		addValues=1
		addFlags=0
	else
		addValues=0
		addFlags=0
	fi
}

end_previous_namespace() {
	if [[ 0 -ne ${#namespace} ]]; then
		namespace=""
	fi
}

begin_new_namespace() {
	temp_macro_file=${f}
	echo "@TYPE_${namespace}" >> ${temp_macro_file}
}

set_map_to_type_namespace() {
	end_previous_namespace
	# Strip off everything on the line up to the'='.
	namespace=${line##*map_to_type=}
	# Remove everything after comma (version info).
	namespace=${namespace%,*}
	# Remove everything after white space space (line may end in " */", for example).
	namespace=${namespace% *}
	namespace=${namespace%	*}

	# Echo the type name into the output file.
	begin_new_namespace
}

set_default_namespace() {
	end_previous_namespace
	# Remove directory info.
	namespace=${f##*/}
	# Remove file extension.
	namespace=${namespace%.*}
	namespace=$(echo "${namespace}Constants" | sed 's/\(.\)/\U\1/')

	# Echo the type name into the output file.
	begin_new_namespace
}

define_flag_macro() {
	# Filter include guards and print the macro as a constant.
	if ! [[ ${line} =~ ${include_guard_regex} ]]; then
		if [[ 0 -eq ${#namespace} ]]; then
			set_default_namespace
		fi
		macro=$(echo "${line}" | awk '{print $2'})
		echo "@MACRO_${macro} 1" >> "${temp_macro_file}"
	fi
}

undef_flag_macro() {
	if [[ 0 -eq ${#namespace} ]]; then
		set_default_namespace
	fi
	# Print a constant of 0 to the macro file.
	macro=$(echo "${line}" | awk '{print $2'})
	echo "@MACRO_${macro} 0" >> "${temp_macro_file}"
}

define_value_macro() {
	if [[ 0 -eq ${#namespace} ]]; then
		set_default_namespace
	fi
	# Print the macro.
	macro=$(echo ${line} | awk '{print $2}')
	echo "@MACRO_${macro} ${macro}" >> "${temp_macro_file}"
}

##
# For each file, create another file with all the macro names in it. This file
# Will include the source file, and be used to find macro values by running the
# c pre processor on it. The results are added to the macroList file containing
# all macro info. The map to type policy associates all of the macros from a file
# with the type specified in the top of the header.
##
parse_namespace_policy_files() {
	# filenum is used to generate an enumerated include guard for each file.
	# The generated include guard is used in case the file path or name contains
	# characters that can't be part of a valid macro definition.
	local filenum=0

	for f in "${annotated_files[@]}"; do
		# Find ddr_options or use the default.
		get_ddr_options

		if [[ 1 == ${addFlags} || 1 == ${addValues} ]]; then
			# gather qualifying macro definitions
			IFS=$'\n'
			macro_list=$(awk '/@ddr_namespace: (map_to_type=|default)/{print}/^[ 	]*#(define|undef)[ 	]+[A-Za-z_][A-Za-z0-9_]*/{print}' ${f})

			# backup the original file
			cp ${f} ${f}.orig

			# Inject repeated include guard and file delimiter
			(( filenum++ ))
			echo >> ${f}
			echo "#ifndef DDRGEN_F${filenum}_GUARD" >> ${f}
			echo "#define DDRGEN_F${filenum}_GUARD" >> ${f}
			echo "@DDRFILE_BEGIN ${f}" >> ${f}

			for line in ${macro_list}; do
				# Regex's used to process the defines and namespace annotations.
				# The script was not happy on Windows if the regex's are in the if statements.
				map_to_type_regex="(@ddr_namespace: map_to_type=)"
				default_regex="(@ddr_namespace: default)"
				flag_define_regex="^[ 	]*\#define[ 	]+[A-Za-z_][A-Za-z0-9_]*[ 	]*($|\/\/|\/\*)"
				flag_undef_regex="^[ 	]*\#undef[ 	]+[A-Za-z_][A-Za-z0-9_]*[ 	]*($|\/\/|\/\*)"
				value_define_regex="^[ 	]*#define[ 	]+[A-Za-z_][A-Za-z0-9_]*[^(][ 	]+.+($|\/\/|\/\*)"
				include_guard_regex="((.*_h)|(.*_hpp)|(.*_h_)|(.*_hpp_))([ \t]*($|\/\/|\/\*))"

				# Remove trailing whitespace from lines. This clears line endings.
				line="$(echo -e "${line}" | sed -e 's/[[:space:]]*$//')"

				if [[ ${line} =~ ${map_to_type_regex} ]]; then
					# Change namespaces to specified pseudostructure.
					set_map_to_type_namespace
				elif [[ ${line} =~ ${default_regex} ]]; then
					# Change namespaces to "<FilenameConstants>".
					set_default_namespace
				elif [[ ${line} =~ ${flag_define_regex} && 1 == ${addFlags} ]]; then
					# Print a define flag as a constant of 1.
					define_flag_macro
				elif [[ ${line} =~ ${flag_undef_regex} && 1 == ${addFlags} ]]; then
					# Print an undef flag as a constant of 0.
					undef_flag_macro
				elif [[ ${line} =~ ${value_define_regex} && 1 == ${addValues} ]]; then
					# Print a value define.
					define_value_macro
				fi
			done
			end_previous_namespace

			# close repeated include guard and file delimiter
			echo "@DDRFILE_END ${f}" >> ${f}
			echo "#endif" >> ${f}
		fi
	done
}

restore_annotated_files() {
	for f in "${annotated_files[@]}"; do
		# Find ddr_options or use the default.
		get_ddr_options

		if [[ 1 == ${addFlags} || 1 == ${addValues} ]]; then
			# restore the original file
			cp ${f}.orig ${f}
			rm ${f}.orig
		fi
	done
}
main() {
	annotated_files=( $(get_all_annotated_files) )

	# Overwrite the output file if it exists. This file will contain all of the final macro/type info.
	echo "" > ${macroList_file}

	echo "Annotating source files containing macros ..."
	# Deal with specific policies.
	parse_namespace_policy_files

	# preprocess annotated source code
	if [[ $(command -v gmake) ]]; then
		gmake --ignore-errors -C ${scan_dir} ddrgen
	else
		make --ignore-errors -C ${scan_dir} ddrgen
	fi

	echo "Scraping anotations from preprocessed code ..."
	local preproc_files=$(grep -rl -e @MACRO_ -e @TYPE_ ${scan_dir} | grep '\.i$')
	if [[ 0 -ne ${#preproc_files} ]]; then
		grep -h -e @DDRFILE -e @MACRO_ -e @TYPE_ ${preproc_files} > ${macroList_file}
	fi

	echo "Restoring annotated files ..."
	restore_annotated_files
}

# Command line arg 1.
scan_dir=${1}
macroList_file="${scan_dir}/macroList"

if [[ $(uname) == *"Win"* ]]; then
	export PATH="/c/dev/products/jtc-toolchain/java7/windows/mingw-msys/msys/1.0/bin:$PATH"
fi

main
