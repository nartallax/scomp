.PHONY: test
test:
	@v -stats test src

.PHONY: coverage
coverage:
	@rm -rf coverage
	@VCH_SRC=src VCH_OUTPUT=coverage v tool v_coverage_helper