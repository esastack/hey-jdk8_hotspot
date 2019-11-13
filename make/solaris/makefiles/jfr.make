#
# Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.  Oracle designates this
# particular file as subject to the "Classpath" exception as provided
# by Oracle in the LICENSE file that accompanied this code.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.
#

include $(GAMMADIR)/make/solaris/makefiles/rules.make

.PHONY: all clean cleanall

TOPDIR      = $(shell echo `pwd`)
GENERATED   = $(TOPDIR)/../generated

JFR_TOOLS_SRCDIR := $(GAMMADIR)/make/src/classes
JFR_TOOLS_OUTPUTDIR := $(GENERATED)/tools/jfr

JFR_GEN_SOURCE := $(JFR_TOOLS_SRCDIR)/build/tools/jfr/GenerateJfrFiles.java
JFR_GEN_CLASS := $(JFR_TOOLS_OUTPUTDIR)/build/tools/jfr/GenerateJfrFiles.class

TOOL_JFR_GEN := $(RUN.JAVA) -cp $(JFR_TOOLS_OUTPUTDIR) build.tools.jfr.GenerateJfrFiles

JFR_OUTPUTDIR := $(GENERATED)/jfrfiles
JFR_SRCDIR := $(GAMMADIR)/src/share/vm/jfr/metadata

# Changing these will trigger a rebuild of generated jfr files.
METADATA_XML := $(JFR_SRCDIR)/metadata.xml
METADATA_XSD := $(JFR_SRCDIR)/metadata.xsd

JFR_GENERATED_FILE := $(JFR_OUTPUTDIR)/jfrEventClasses.hpp

all: $(JFR_GENERATED_FILE)

clean:
	rm $(JFR_GEN_CLASS) $(JFR_OUTPUTDIR)

cleanall:
	rm $(JFR_GEN_CLASS) $(JFR_OUTPUTDIR)


$(JFR_GEN_CLASS): $(JFR_GEN_SOURCE)
	@echo Generating $(@)
	mkdir -p $(@D)
	$(QUIETLY) $(REMOTE) $(COMPILE.JAVAC) -d $(JFR_TOOLS_OUTPUTDIR) $(JFR_GEN_SOURCE)
	test -f $(@)

$(JFR_GENERATED_FILE): $(METADATA_XML) $(METADATA_XSD) $(JFR_GEN_CLASS)
	@echo Generating $(@)
	mkdir -p $(@D)
	$(QUIETLY) $(REMOTE) $(TOOL_JFR_GEN) $(METADATA_XML) $(METADATA_XSD) $(@D)
	test -f $(@)
