(function($) {

    /**
     * @param {object} $el
     * @param {string|array} val
     */
    var sliderChangeHandler = function($el, val) {

        //Indicate the type of operation made on the slider
        var operationFieldName = $el.attr("data-identifier") + '\\[op\\]';
        var $operationField = $('input[name='+operationFieldName+']');
        if($operationField.length == 1) {
            console.log(val);
            var operation = "change";
            if( Object.prototype.toString.call( val ) === '[object Array]' ) {
                var min = parseFloat(val[0]);
                var max = parseFloat(val[1]);
                var rangeMin = parseFloat($el.attr("data-slider-min"));
                var rangeMax = parseFloat($el.attr("data-slider-max"));
                if(min==rangeMin && max==rangeMax) {
                    operation = "unset";
                }
            }
            $operationField.val(operation);
        }

        // Submit form by clicking on the submit button (only if autosubmit)
        if(val) {
            $el.closest("form").find('.ctools-auto-submit-click').click();
        }
    };

    /**
     * Init sliders
     */
    var initializeSliders = function() {
        var $sliders = $("input[type=text].bootstrap-slider");
        if($sliders.length > 0) {
            $sliders.each(function() {
                $(this).slider().on('slideStop', function(e) {
                    sliderChangeHandler($(this), e.value);
                });
            })
        }
    };

    $(document).ready(function() {
        initializeSliders();
    });

    $(document).ajaxComplete(function() {
        initializeSliders();
    });
})(jQuery);