(function ($) {
    Drupal.behaviors.commerceOmnipay = {
        attach: function (context, settings) {
            $('.payment_gateway').click(function() {
                var element = $(this);
                var checked = element.prop('checked');
                $('.payment_gateway').prop('checked', false);
                element.prop('checked', checked);
            });

            $('#commerce-omnipay-settings-form fieldset legend').click(function() {
                var fieldset = $(this).parent('fieldset.collapsible');

                $(fieldset).children('.panel-body').slideToggle();
                $(fieldset).toggleClass('show_body');
            })
        }
    };
})(jQuery);
