(function ($) {
    Drupal.behaviors.commerceOmnipay = {
        attach: function (context, settings) {
            $('.payment_gateway').click(function() {
                var element = $(this);
                var checked = element.prop('checked');
                $('.payment_gateway').prop('checked', false);
                element.prop('checked', checked);
            });
        }
    };
})(jQuery);
