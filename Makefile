# PS4 Downloader - modeled on OpenOrbis sample Makefiles. Needs OO_PS4_TOOLCHAIN set.
TITLE      := PS4 Downloader
VERSION    := 1.10
TITLE_ID   := MYDL00001
CONTENT_ID := IV0000-MYDL00001_00-PS4DOWNLOADER000

LIBS := -lc -lkernel -lSceNet -lSceSsl -lSceHttp -lScePad -lSceUserService -lSceVideoOut -lSceSystemService

TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
ifeq ($(shell uname -s),Darwin)
  OSDIR := macos
else
  OSDIR := linux
endif
BIN := $(TOOLCHAIN)/bin/$(OSDIR)
INTDIR := build
OUTDIR := out

CC := clang
LD := ld.lld
CFLAGS := --target=x86_64-pc-freebsd12-elf -fPIC -O2 -funwind-tables -c -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -Isrc
LDFLAGS := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(INTDIR)/%.o,$(SRCS))

all: $(OUTDIR)/$(CONTENT_ID).pkg

$(INTDIR)/%.o: src/%.c $(wildcard src/*.h)
	@mkdir -p $(INTDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/app.elf: $(OBJS)
	$(LD) $(OBJS) -o $@ $(LDFLAGS)

eboot.bin: $(INTDIR)/app.elf
	$(BIN)/create-fself -in=$< --out=$(INTDIR)/app.oelf --eboot=eboot.bin --paid 0x3800000000000011

sce_sys/about/right.sprx:
	@mkdir -p sce_sys/about
	cp "$(TOOLCHAIN)/samples/hello_world/sce_sys/about/right.sprx" $@

sce_sys/param.sfo: Makefile
	$(BIN)/PkgTool.Core sfo_new $@
	$(BIN)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 0
	$(BIN)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(BIN)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(BIN)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(BIN)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(BIN)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(BIN)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(BIN)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(BIN)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(BIN)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png
	$(BIN)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

$(OUTDIR)/$(CONTENT_ID).pkg: pkg.gp4
	@mkdir -p $(OUTDIR)
	$(BIN)/PkgTool.Core pkg_build $< $(OUTDIR)

clean:
	rm -rf $(INTDIR) $(OUTDIR) eboot.bin pkg.gp4 sce_sys/param.sfo
.PHONY: all clean
