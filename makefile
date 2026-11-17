.PHONY: test
test:
	@rm -rf ~/.vmodules/.cache
	@v -W test src

.PHONY: coverage
coverage:
	@rm -rf ~/.vmodules/.cache
	@rm -rf coverage
	@VCH_SRC=src VCH_OUTPUT=coverage v tool v_coverage_helper