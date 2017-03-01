BUILD_TYPE?=Release
BUILD_DIR?=build
BUILD_REPOS?=git@github.com:OpenChannelSSD/pblk-tools.git

#
# Traditional build commands / make interface
#
default: make

.PHONY: debug
debug:
	$(eval BUILD_TYPE := Debug)

.PHONY: cmake_check
cmake_check:
	@cmake --version || (echo "\n** Please install 'cmake' **\n" && exit 1)

.PHONY: configure
configure: cmake_check
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) ../src
	@echo "Modify build configuration in '$(BUILD_DIR)'"

.PHONY: make
make: configure
	cd $(BUILD_DIR) && make

.PHONY: install
install:
	cd $(BUILD_DIR) && make install

.PHONY: clean
clean:
	rm -fr $(BUILD_DIR) || true

all: clean default install

#
# Extension: package generation
#
.PHONY: make-pkg
make-pkg: configure
	cd $(BUILD_DIR) && make package

.PHONY: install-pkg
install-pkg:
	sudo dpkg -i $(BUILD_DIR)/*.deb

.PHONY: uninstall-pkg
uninstall-pkg:
	sudo apt-get --yes remove nvm_pblk || true


#
# Extension: commands useful in development cycle
#
.PHONY: dev
dev: uninstall-pkg clean make make-pkg install-pkg

#
# Extension: documentation generation
#
.PHONY: doc
doc:
	@mkdir -p $(BUILD_DIR)/doc/sphinx/html
	sphinx-build -b html -c doc -E doc/src $(BUILD_DIR)/doc/sphinx/html

.PHONY: sphinx-view
doc-view:
	xdg-open $(BUILD_DIR)/doc/sphinx/html/index.html

.PHONY: doc-publish
doc-publish:
	rm -rf $(BUILD_DIR)/ghpages
	mkdir -p $(BUILD_DIR)/ghpages
	git clone -b gh-pages $(BUILD_REPOS) --single-branch $(BUILD_DIR)/ghpages
	cd $(BUILD_DIR)/ghpages && git rm -rf --ignore-unmatch .
	cp -r $(BUILD_DIR)/doc/sphinx/html/. $(BUILD_DIR)/ghpages/
	touch $(BUILD_DIR)/ghpages/.nojekyll
	cd $(BUILD_DIR)/ghpages && git add .
	if [ -z "`git config user.name`" ]; then git config user.name "Mr. Robot"; fi
	if [ -z "`git config user.email`" ]; then git config user.email "foo@example.com"; fi
	cd $(BUILD_DIR)/ghpages && git commit -m "Autogen docs for `git rev-parse --short HEAD`."
	cd $(BUILD_DIR)/ghpages && git push origin --delete gh-pages
	cd $(BUILD_DIR)/ghpages && git push origin gh-pages
